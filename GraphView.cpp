#include "GraphView.h"
#include <QPen>
#include <QBrush>
#include <QDebug>
#include <QTimer>


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
    nodeItem.clear();
    edgeItem.clear();

    const qreal R = 30.0;
    mNodeRadius = R;

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
        // 链接热启动
        connect(node, &NodeItem::dragStarted, this, [this]() {
            heatUp(1.0);
        });
        connect(node, &NodeItem::dragEnded, this, [this]() {
            heatUp(0.6); // 放开后也“回温”一下，让它继续自然回弹
        });
        connect(node, &NodeItem::pinChanged, this, [this](bool) {
            heatUp(0.8);
        });

    }

    // 2) 再建边（EdgeItem 绑定两个 NodeItem，节点移动会触发 updatePath）
    for (auto [u, v] : g.edges) {
        if (!nodeItem.contains(u) || !nodeItem.contains(v)) continue;

        auto* e = new EdgeItem(nodeItem[u], nodeItem[v], R);
        mScene->addItem(e);
        edgeItem[{u, v}] = e;
    }

    // 3) 视图自适应（保留你原来的逻辑）
    QRectF rect = mScene->itemsBoundingRect();
    rect = rect.adjusted(-80, -80, 80, 80);
    mScene->setSceneRect(rect);

    resetTransform();
    lastRect = rect;

    QTimer::singleShot(0, this, [this]() {
        if (!lastRect.isNull())
            fitInView(lastRect, Qt::KeepAspectRatio);
    });

    mLayoutBounds = mScene->itemsBoundingRect().adjusted(-200, -200, 200, 200);
    mScene->setSceneRect(mLayoutBounds);

    if (mForceEnabled) startForceLayout();
    //末尾布局启动前，把速度清零、alpha 回到 1：
    mAlpha = 1.0;
    for (auto it = nodeItem.begin(); it != nodeItem.end(); ++it) {
        it.value()->vel = QPointF(0,0);
    }
    if (mForceEnabled) startForceLayout();

}


void GraphView::resizeEvent(QResizeEvent* event)
{
    QGraphicsView::resizeEvent(event);
    if (!lastRect.isNull()) {
        fitInView(lastRect, Qt::KeepAspectRatio);
    }
}


void GraphView::resetStyle()
{
    // 先留空：后面做高亮/颜色的时候再写
}

void GraphView::applyStep(const Step& step)
{
    // 先留空：后面做播放 steps 的时候再写
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
        QPointF f = dir * (mSpringK * stretch);

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
        const double margin = 40.0;
        QRectF r = mLayoutBounds;
        np.setX(std::min(r.right()  - margin, std::max(r.left() + margin, np.x())));
        np.setY(std::min(r.bottom() - margin, std::max(r.top()  + margin, np.y())));

        n->setPos(np);
    }
}

