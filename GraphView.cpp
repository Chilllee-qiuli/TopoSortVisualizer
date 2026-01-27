#include "GraphView.h"
#include <QPen>
#include <QBrush>
#include <QDebug>
#include <QTimer>
#include <QMouseEvent>

GraphView::GraphView(QWidget* parent)
    : QGraphicsView(parent)
{
    mScene = new QGraphicsScene(this);
    setScene(mScene);
    connect(&mForceTimer, &QTimer::timeout, this, &GraphView::onForceTick);
    mForceTimer.setInterval(16); // ~60FPS
    if (mForceEnabled) mForceTimer.start();
    setRenderHint(QPainter::Antialiasing, true);
    // Qt 有时候窗口 resize 或滚轮缩放会让视图锚点变化，导致看起来缩得很奇怪，这两行能让缩放/resize 行为更稳定。
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);

}

void GraphView::showGraph(const Graph& g, const QVector<QPointF>& pos)
{
    mScene->clear();
    // 避免空指针
    mArenaItem = nullptr;
    mEdgeWeight.clear();
    mNodeCountHint = g.n;
    nodeItem.clear();
    edgeItem.clear();

    const qreal R = 30.0;
    mNodeRadius = R;

    // 清理 step 相关状态
    mActiveNode = -1;
    mActiveEdge = {-1, -1};
    mTopoOrderIndex = 0;
    mBaseBrush.clear();
    mBasePen.clear();
    mBaseEdgePen.clear();
    mIndegText.clear();
    mOrderText.clear();


    // 1) 先建节点（必须先建，因为边要拿到节点指针）
    for (int i = 1; i <= g.n; i++) {
        if (i <= 0 || i >= pos.size()) continue;

        auto* node = new NodeItem(i, R);
        node->setPen(QPen(Qt::black, 2));
        node->setBrush(QBrush(Qt::white));
        node->setPos(pos[i]);

        // 文字做成 node 的子 item：节点动，文字自动跟随
        auto* label = new QGraphicsSimpleTextItem(QString::number(i), node);
        QRectF br = label->boundingRect();
        label->setPos(-br.width() / 2.0, -br.height() / 2.0);

        mScene->addItem(node);
        nodeItem[i] = node;

        // 记录默认样式
        mBaseBrush[i] = node->brush();
        mBasePen[i]   = node->pen();

        // 链接热启动
        connect(node, &NodeItem::dragStarted, this, [this]() {
            heatUp(1.0);
        });
        connect(node, &NodeItem::dragEnded, this, [this, node]() {
            clampNodeToArena(node);
            heatUp(0.6); // 放开后也“回温”一下，让它继续自然回弹
            // 防止手把点拖到场外
        });
        connect(node, &NodeItem::pinChanged, this, [this](bool) {
            heatUp(0.8);
        });

    }

    // 2) 再建边（EdgeItem 绑定两个 NodeItem，节点移动会触发 updatePath）
    for (auto [u, v] : g.edges) {
        if (!nodeItem.contains(u) || !nodeItem.contains(v)) continue;

        auto* e = new EdgeItem(nodeItem[u], nodeItem[v], R);

        // 之后 applyStep() 会改颜色、线宽。如果不保存 base style，就没法恢复原样。
        mScene->addItem(e);
        edgeItem[{u, v}] = e;
        mBaseEdgePen[{u, v}] = e->pen(); // 记录默认边样式

        mEdgeWeight[{u, v}] = 1.0; // 已有边=满强度
    }

    // // 3) 视图自适应（保留你原来的逻辑）
    // QRectF rect = mScene->itemsBoundingRect();
    // rect = rect.adjusted(-80, -80, 80, 80);
    // mScene->setSceneRect(rect);

    // resetTransform();
    // lastRect = rect;

    // QTimer::singleShot(0, this, [this]() {
    //     if (!lastRect.isNull())
    //         fitInView(lastRect, Qt::KeepAspectRatio);
    // });

    // mLayoutBounds = mScene->itemsBoundingRect().adjusted(-200, -200, 200, 200);
    // mScene->setSceneRect(mLayoutBounds);

    updateArena(g.n);

    resetTransform();
    QTimer::singleShot(0, this, [this]() {
        fitInView(lastRect, Qt::KeepAspectRatio);
    });



    //末尾布局启动前，把速度清零、alpha 回到 1：
    mAlpha = 1.0;
    for (auto it = nodeItem.begin(); it != nodeItem.end(); ++it) {
        it.value()->vel = QPointF(0,0);
    }
    if (mForceEnabled) startForceLayout();

}

// 把 lastRect 统一成 arena,让 arena 不小于窗口：
void GraphView::resizeEvent(QResizeEvent* event)
{
    QGraphicsView::resizeEvent(event);
    updateArena(mNodeCountHint);
    fitInView(lastRect, Qt::KeepAspectRatio);
}



void GraphView::resetStyle()
{
    mActiveNode = -1;
    mActiveEdge = {-1, -1};
    mTopoOrderIndex = 0;

    // 恢复节点样式
    for (auto it = nodeItem.begin(); it != nodeItem.end(); ++it) {
        int id = it.key();
        NodeItem* n = it.value();
        if (!n) continue;
        n->setBrush(mBaseBrush.value(id, QBrush(Qt::white)));
        n->setPen(mBasePen.value(id, QPen(Qt::black, 2)));
    }

    // 恢复边样式
    for (auto it = edgeItem.begin(); it != edgeItem.end(); ++it) {
        auto key = it.key();
        EdgeItem* e = it.value();
        if (!e) continue;
        e->setPen(mBaseEdgePen.value(key, QPen(Qt::black, 2)));
    }

    // 清掉入度/序号文字（它们是 node 的子 item，删掉最稳）
    for (auto t : mIndegText)  delete t;
    for (auto t : mOrderText)  delete t;
    mIndegText.clear();
    mOrderText.clear();
}


static QColor sccPaletteColor(int scc)
{
    // 一个简单可重复的配色：不同 scc 得到不同 hue
    int hue = (scc * 47) % 360;
    return QColor::fromHsv(hue, 80, 255);
}

void GraphView::applyStep(const Step& step)
{
    auto unhighlightNode = [&](int id){
        if (id < 0 || !nodeItem.contains(id)) return;
        NodeItem* n = nodeItem[id];
        if (!n) return;
        n->setPen(mBasePen.value(id, QPen(Qt::black, 2)));
    };

    auto highlightNode = [&](int id){
        if (id < 0 || !nodeItem.contains(id)) return;
        if (mActiveNode != -1 && mActiveNode != id) unhighlightNode(mActiveNode);
        mActiveNode = id;
        NodeItem* n = nodeItem[id];
        if (!n) return;
        n->setPen(QPen(QColor(255, 140, 0), 4)); // 橙色粗边框
    };

    auto unhighlightEdge = [&](QPair<int,int> ekey){
        if (!edgeItem.contains(ekey)) return;
        EdgeItem* e = edgeItem[ekey];
        if (!e) return;
        e->setPen(mBaseEdgePen.value(ekey, QPen(Qt::black, 2)));
    };

    auto highlightEdge = [&](QPair<int,int> ekey){
        if (!edgeItem.contains(ekey)) return;
        if (mActiveEdge.first != -1 && mActiveEdge != ekey) unhighlightEdge(mActiveEdge);
        mActiveEdge = ekey;
        EdgeItem* e = edgeItem[ekey];
        if (!e) return;
        e->setPen(QPen(QColor(220, 20, 60), 3)); // 红色高亮边
    };

    auto ensureIndegText = [&](int id, int val){
        if (id < 0 || !nodeItem.contains(id)) return;
        NodeItem* n = nodeItem[id];
        if (!n) return;

        QGraphicsSimpleTextItem* t = mIndegText.value(id, nullptr);
        if (!t) {
            t = new QGraphicsSimpleTextItem("", n);
            mIndegText[id] = t;
        }
        t->setText(QString("in:%1").arg(val));
        QRectF br = t->boundingRect();
        t->setPos(-br.width()/2.0, mNodeRadius - br.height()); // 放在节点下半部
    };

    auto ensureOrderText = [&](int id, int order){
        if (id < 0 || !nodeItem.contains(id)) return;
        NodeItem* n = nodeItem[id];
        if (!n) return;

        QGraphicsSimpleTextItem* t = mOrderText.value(id, nullptr);
        if (!t) {
            t = new QGraphicsSimpleTextItem("", n);
            mOrderText[id] = t;
        }
        t->setText(QString("#%1").arg(order));
        QRectF br = t->boundingRect();
        t->setPos(mNodeRadius - br.width(), -mNodeRadius); // 右上角
    };

    switch (step.type) {
    case StepType::Visit: {
        highlightNode(step.u);
        // 可选：Visit 时轻微变黄（不覆盖 base，只临时改 brush）
        if (nodeItem.contains(step.u)) {
            nodeItem[step.u]->setBrush(QBrush(QColor(255, 255, 200)));
        }
        break;
    }
    case StepType::AssignSCC: {
        highlightNode(step.u);
        QColor c = sccPaletteColor(step.scc);
        // AssignSCC 属于“持久状态”：更新 baseBrush，这样后面高亮恢复也还是 SCC 色
        mBaseBrush[step.u] = QBrush(c);
        if (nodeItem.contains(step.u)) nodeItem[step.u]->setBrush(mBaseBrush[step.u]);
        break;
    }
    case StepType::TopoInitIndeg: {
        // step.u 是节点，step.val 是 indeg
        ensureIndegText(step.u, step.val);
        break;
    }
    case StepType::TopoEnqueue: {
        highlightNode(step.u);
        // 入队也算“持久状态”（直到出队），更新 baseBrush
        mBaseBrush[step.u] = QBrush(QColor(200, 255, 200));
        if (nodeItem.contains(step.u)) nodeItem[step.u]->setBrush(mBaseBrush[step.u]);
        break;
    }
    case StepType::TopoDequeue: {
        highlightNode(step.u);
        mTopoOrderIndex++;
        ensureOrderText(step.u, mTopoOrderIndex);

        // 出队 -> 已输出：更新 baseBrush
        mBaseBrush[step.u] = QBrush(QColor(200, 220, 255));
        if (nodeItem.contains(step.u)) nodeItem[step.u]->setBrush(mBaseBrush[step.u]);
        break;
    }

    // 顺手支持这俩（可留着以后再开）
    case StepType::TopoIndegDec: {
        // step.v 是被减入度的点，step.val 是减完后的 indeg
        ensureIndegText(step.v, step.val);
        highlightEdge({step.u, step.v});
        break;
    }
    case StepType::BuildCondensedEdge: {
        highlightEdge({step.u, step.v});
        break;
    }

    default:
        break;
    }
}

void GraphView::startForceLayout() {
    if (!mForceTimer.isActive()) mForceTimer.start();
}

void GraphView::stopForceLayout() {
    mForceTimer.stop();
}

void GraphView::setForceEnabled(bool on) {
    mForceEnabled = on;
    if (on) startForceLayout();
    else stopForceLayout();
}
void GraphView::onForceTick()
{
    if (!mScene || nodeItem.isEmpty()) return;

    // cooling
    mAlpha *= (1.0 - mAlphaDecay);
    if (mAlpha < mAlphaMin) {
        stopForceLayout();
        return;
    }

    QMap<int, QPointF> force;
    for (auto it = nodeItem.begin(); it != nodeItem.end(); ++it)
        force[it.key()] = QPointF(0, 0);

    QList<int> ids = nodeItem.keys();

    // 每 tick 让新边权重更接近 1（让新边更顺滑）
    for (auto it = mEdgeWeight.begin(); it != mEdgeWeight.end(); ++it) {
        it.value() = std::min(1.0, it.value() + 0.08);
    }

    // 2) 点-点排斥 + 防重叠
    for (int a = 0; a < ids.size(); ++a) {
        for (int b = a + 1; b < ids.size(); ++b) {
            int i = ids[a], j = ids[b];
            NodeItem* ni = nodeItem[i];
            NodeItem* nj = nodeItem[j];
            if (!ni || !nj) continue;

            QPointF d = ni->pos() - nj->pos();
            double dx = d.x(), dy = d.y();
            double dist2 = dx*dx + dy*dy + 1e-3;
            double dist  = std::sqrt(dist2);

            QPointF dir = d / dist;
            double mag = mRepulsion / dist2;       // 1/r^2
            QPointF f = dir * mag;

            // collision: dist < 2R 时额外弹开
            double minDist = 2.0 * mNodeRadius + 6.0;
            if (dist < minDist) {
                double push = (minDist - dist) * mCollisionK;
                f += dir * push * 50.0; // 50 是经验放大，可再调
            }

            force[i] += f;
            force[j] -= f;
        }
    }

    // 3) 边弹簧
    for (auto it = edgeItem.begin(); it != edgeItem.end(); ++it) {
        int u = it.key().first;
        int v = it.key().second;
        if (!nodeItem.contains(u) || !nodeItem.contains(v)) continue;

        NodeItem* nu = nodeItem[u];
        NodeItem* nv = nodeItem[v];
        if (!nu || !nv) continue;

        QPointF d = nv->pos() - nu->pos();
        double dx = d.x(), dy = d.y();
        double dist2 = dx*dx + dy*dy + 1e-3;
        double dist  = std::sqrt(dist2);

        QPointF dir = d / dist;
        double stretch = dist - mRestLen;
        double w = mEdgeWeight.value({u, v}, 1.0);
        QPointF f = dir * (mSpringK * w * stretch);

        force[u] += f;
        force[v] -= f;
    }

    // 4) 向中心轻微拉力
    QPointF center = mScene->sceneRect().center();
    for (int id : ids) {
        NodeItem* n = nodeItem[id];
        if (!n) continue;
        force[id] += (center - n->pos()) * mCenterPull;
    }

    // 乘上 alpha：越到后面越“冷”
    for (int id : ids) force[id] *= mAlpha;

    QGraphicsItem* grabbed = mScene->mouseGrabberItem();

    for (int id : ids) {
        NodeItem* n = nodeItem[id];
        if (!n) continue;

        bool dragging = false;
        if (grabbed) dragging = (grabbed == n) || (grabbed->parentItem() == n);

        // 拖拽时临时固定；pin 的点永久固定
        if (n->pinned() || dragging) {
            n->vel = QPointF(0,0);
            continue;
        }

        // 速度更新（保留 dt 用一次就够）
        n->vel = (n->vel + force[id] * mDt) * mDamping;

        // 限速
        double sp = std::hypot(n->vel.x(), n->vel.y());
        if (sp > mMaxSpeed) n->vel *= (mMaxSpeed / sp);

        // 关键：位移不要再乘 dt（否则太慢）
        QPointF np = n->pos() + n->vel;

        // 边界约束
        const double margin = mNodeRadius + 12;

        QRectF r = mLayoutBounds;
        np.setX(std::min(r.right()  - margin, std::max(r.left() + margin, np.x())));
        np.setY(std::min(r.bottom() - margin, std::max(r.top()  + margin, np.y())));

        n->setPos(np);
    }
}

bool GraphView::addEdge(int u, int v)
{
    if (!mScene) return false;
    if (!nodeItem.contains(u) || !nodeItem.contains(v)) return false;
    if (edgeItem.contains({u, v})) return false;

    auto* e = new EdgeItem(nodeItem[u], nodeItem[v], mNodeRadius);
    mScene->addItem(e);
    edgeItem[{u, v}] = e;

    mEdgeWeight[{u, v}] = 0.0;  // 新边从 0 开始慢慢增强
    heatUp(1.0);                // reheat
    startForceLayout();         // 确保 timer 在跑
    return true;
}
void GraphView::mousePressEvent(QMouseEvent* event)
{
    bool wantAddEdge = mEdgeEditMode || (event->modifiers() & Qt::ShiftModifier);

    if (wantAddEdge && event->button() == Qt::LeftButton) {
        QGraphicsItem* it = itemAt(event->pos());
        NodeItem* node = nullptr;

        if (it) {
            node = dynamic_cast<NodeItem*>(it);
            if (!node && it->parentItem())
                node = dynamic_cast<NodeItem*>(it->parentItem()); // 点到文字时
        }

        // 点空白：取消
        if (!node) {
            if (mEdgeFromNode) mEdgeFromNode->setBrush(QBrush(Qt::white));
            mEdgeFrom = -1; mEdgeFromNode = nullptr;
            if (mPreviewLine) { delete mPreviewLine; mPreviewLine = nullptr; }
            event->accept();
            return;
        }

        if (mEdgeFrom == -1) {
            // 选起点
            mEdgeFrom = node->id();
            mEdgeFromNode = node;
            mEdgeFromNode->setBrush(QBrush(QColor(255, 255, 200))); // 高亮一下

            if (!mPreviewLine) {
                mPreviewLine = mScene->addLine(QLineF(node->pos(), node->pos()),
                                               QPen(Qt::gray, 2, Qt::DashLine));
                mPreviewLine->setZValue(-2);
            } else {
                mPreviewLine->setLine(QLineF(node->pos(), node->pos()));
            }
        } else {
            // 选终点，发请求
            int from = mEdgeFrom;
            int to = node->id();

            if (mEdgeFromNode) mEdgeFromNode->setBrush(QBrush(Qt::white));
            mEdgeFrom = -1; mEdgeFromNode = nullptr;

            if (mPreviewLine) { delete mPreviewLine; mPreviewLine = nullptr; }

            if (from != to) emit edgeRequested(from, to);
        }

        heatUp(1.0);
        event->accept();
        return;
    }

    QGraphicsView::mousePressEvent(event);
}

void GraphView::mouseMoveEvent(QMouseEvent* event)
{
    if (mPreviewLine && mEdgeFromNode) {
        QPointF p = mapToScene(event->pos());
        mPreviewLine->setLine(QLineF(mEdgeFromNode->pos(), p));
    }
    QGraphicsView::mouseMoveEvent(event);
}

void GraphView::leaveEvent(QEvent* event)
{
    // 鼠标离开视图就取消预览
    if (mEdgeFromNode) mEdgeFromNode->setBrush(QBrush(Qt::white));
    mEdgeFrom = -1; mEdgeFromNode = nullptr;
    if (mPreviewLine) { delete mPreviewLine; mPreviewLine = nullptr; }

    QGraphicsView::leaveEvent(event);
}
void GraphView::updateArena(int n)
{
    if (!mScene) return;
    if (n <= 0) n = nodeItem.size();

    // arena 边长随 n 增大（经验公式：sqrt(n) 级别）
    double sideByN = std::sqrt((double)n) * (mRestLen * 3.0);
    double side = std::max(900.0, sideByN);

    // 至少不小于当前 viewport（避免“看起来跑出去”）
    side = std::max(side, (double)viewport()->width());
    side = std::max(side, (double)viewport()->height());

    mLayoutBounds = QRectF(-side/2, -side/2, side, side);
    mScene->setSceneRect(mLayoutBounds);

    // 画一个可见边框（你说“场地没边界”就靠它）
    if (!mArenaItem) {
        mArenaItem = mScene->addRect(mLayoutBounds, QPen(QColor(200,200,200), 2, Qt::DashLine));
        mArenaItem->setZValue(-10);
    } else {
        mArenaItem->setRect(mLayoutBounds);
    }

    lastRect = mLayoutBounds; // ✅关键：以后 fitInView 就用这个
}

void GraphView::clampNodeToArena(NodeItem* n)
{
    if (!n) return;
    const double margin = mNodeRadius + 12;
    QRectF r = mLayoutBounds;
    QPointF p = n->pos();
    p.setX(std::min(r.right()  - margin, std::max(r.left() + margin, p.x())));
    p.setY(std::min(r.bottom() - margin, std::max(r.top()  + margin, p.y())));
    n->setPos(p);
}
