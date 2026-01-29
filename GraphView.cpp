#include "GraphView.h"
#include <QPen>
#include <QBrush>
#include <QDebug>
#include <QTimer>
#include <QMouseEvent>
#include <QColor>
#include <QVariant>

namespace {
/**
 * Visualization state is stored on each QGraphicsItem using setData()/data().
 *
 * Why:
 *  - Avoids intrusive changes to NodeItem/EdgeItem class definitions.
 *  - Keeps algorithms UI-agnostic: algorithms only emit Steps.
 *  - Allows deterministic redraw by calling resetStyle() at any time.
 */
constexpr int kRoleSccId   = 0x100;   // int: assigned SCC id, 0 = unassigned
constexpr int kRoleInStack = 0x101;   // bool: Tarjan stack membership
constexpr int kRoleActive  = 0x102;   // bool: transient highlight for the current step

static QColor sccColor(int sccId) {
    // Deterministic vivid palette via HSV. Stable across repaint/relayout.
    const int hue = (sccId * 47) % 360;
    return QColor::fromHsv(hue, 160, 255);
}

static void styleNode(NodeItem* node) {
    if (!node) return;

    const int  sccId   = node->data(kRoleSccId).toInt();
    const bool inStack = node->data(kRoleInStack).toBool();
    const bool active  = node->data(kRoleActive).toBool();

    // Outline priority: active (red) > inStack (orange) > default (black)
    QPen pen(Qt::black, 2);
    if (active) {
        pen = QPen(QColor(220, 40, 40), 4);
    } else if (inStack) {
        pen = QPen(QColor(255, 140, 0), 3);
    }
    node->setPen(pen);

    // Fill: SCC color is persistent once assigned; otherwise keep white.
    if (sccId > 0) node->setBrush(QBrush(sccColor(sccId)));
    else node->setBrush(QBrush(Qt::white));
}
} // namespace

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
    // Keep the original API for existing callers.
    showGraphEx(g, pos, QStringList(), QVector<int>());

}

void GraphView::showGraphEx(const Graph& g,
                            const QVector<QPointF>& pos,
                            const QStringList& labels,
                            const QVector<int>& colorId)
{
    // Scene rebuild is intentional:
    //  - It avoids incremental "diff" logic across modes (Original vs DAG).
    //  - It guarantees a deterministic state after each phase transition.
    mScene->clear();
    nodeItem.clear();
    edgeItem.clear();
    mEdgeWeight.clear();
    mIndegText.clear();
    mOrderText.clear();

    const qreal R = 30.0;
    mNodeRadius = R;

    // 1) Nodes first (edges need pointers to NodeItem).
    for (int i = 1; i <= g.n; i++) {
        if (i <= 0 || i >= pos.size()) continue;

        auto* node = new NodeItem(i, R);
        node->setPen(QPen(Qt::black, 2));
        node->setPos(pos[i]);

        // Initialize per-node visualization state.
        const int cid = (i < colorId.size()) ? colorId[i] : 0;
        node->setData(kRoleSccId, cid);
        node->setData(kRoleInStack, false);
        node->setData(kRoleActive, false);

        // Label follows the node (child item).
        const QString text = (i < labels.size() && !labels[i].isEmpty())
                                 ? labels[i]
                                 : QString::number(i);
        auto* label = new QGraphicsSimpleTextItem(text, node);
        QRectF br = label->boundingRect();
        label->setPos(-br.width() / 2.0, -br.height() / 2.0);

        mScene->addItem(node);
        nodeItem[i] = node;

        // Reheat physics when user interacts.
        connect(node, &NodeItem::dragStarted, this, [this]() { heatUp(1.0); });
        connect(node, &NodeItem::dragEnded,   this, [this]() { heatUp(0.6); });
        connect(node, &NodeItem::pinChanged,  this, [this](bool) { heatUp(0.8); });
    }

    // 2) Edges.
    for (auto [u, v] : g.edges) {
        if (!nodeItem.contains(u) || !nodeItem.contains(v)) continue;
        auto* e = new EdgeItem(nodeItem[u], nodeItem[v], R);
        mScene->addItem(e);
        edgeItem[{u, v}] = e;
        mEdgeWeight[{u, v}] = 1.0; // Existing edges start at full strength.
    }

    // 3) Re-render all items (e.g. SCC palette fills).
    resetStyle();

    // 4) View framing & force layout bootstrap.
    QRectF rect = mScene->itemsBoundingRect();
    rect = rect.adjusted(-80, -80, 80, 80);
    mScene->setSceneRect(rect);

    resetTransform();
    lastRect = rect;

    QTimer::singleShot(0, this, [this]() {
        if (!lastRect.isNull()) fitInView(lastRect, Qt::KeepAspectRatio);
    });

    mLayoutBounds = mScene->itemsBoundingRect().adjusted(-200, -200, 200, 200);
    mScene->setSceneRect(mLayoutBounds);

    // Reset simulation state for a clean "settle" after rebuild.
    mAlpha = 1.0;
    for (auto it = nodeItem.begin(); it != nodeItem.end(); ++it) {
        it.value()->vel = QPointF(0, 0);
    }
    if (mForceEnabled) startForceLayout();
}

QVector<QPointF> GraphView::snapshotPositions(int n) const
{
    QVector<QPointF> pos(n + 1);
    for (int i = 1; i <= n; ++i) {
        // QMap::operator[] is non-const; use value() for a const-safe lookup.
        NodeItem* node = nodeItem.value(i, nullptr);
        if (node) pos[i] = node->pos();
    }
    return pos;

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
    // Re-render all nodes based on visualization state stored in item data.
    for (auto it = nodeItem.begin(); it != nodeItem.end(); ++it) {
        styleNode(it.value());
    }
}

void GraphView::applyStep(const Step& step)
{
    // Apply a single algorithm step to the scene.
    //
    // Design:
    //  - Persistent state (SCC id / stack membership) is stored per-node using item data roles.
    //  - Step application only mutates these roles, then calls resetStyle() to re-render.
    //  - This keeps algorithm code decoupled from Qt UI code and makes playback deterministic.

    // Clear previous transient highlight.
    for (auto it = nodeItem.begin(); it != nodeItem.end(); ++it) {
        it.value()->setData(kRoleActive, false);
    }

    // Special: Reset visualization state.
    if (step.type == StepType::ResetVisual) {
        const bool clearScc = (step.val != 0);
        for (auto it = nodeItem.begin(); it != nodeItem.end(); ++it) {
            NodeItem* node = it.value();
            if (!node) continue;
            node->setData(kRoleActive, false);
            node->setData(kRoleInStack, false);
            if (clearScc) node->setData(kRoleSccId, 0);
        }
        resetStyle();
        return;
    }

    NodeItem* nodeU = nodeItem.contains(step.u) ? nodeItem[step.u] : nullptr;

    switch (step.type) {
    case StepType::Visit:
        if (nodeU) nodeU->setData(kRoleActive, true);
        break;
    case StepType::PushStack:
        if (nodeU) {
            nodeU->setData(kRoleInStack, true);
            nodeU->setData(kRoleActive, true);
        }
        break;
    case StepType::PopStack:
        if (nodeU) {
            nodeU->setData(kRoleInStack, false);
            nodeU->setData(kRoleActive, true);
        }
        break;
    case StepType::AssignSCC:
        if (nodeU) {
            nodeU->setData(kRoleSccId, step.scc);
            nodeU->setData(kRoleInStack, false);
            nodeU->setData(kRoleActive, true);
        }
        break;
    default:
        // Not handled in this milestone. Keep as no-op for forward compatibility.
        break;
    }

    resetStyle();
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
        const double margin = 40.0;
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
