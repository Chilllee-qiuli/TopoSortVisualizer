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
 * 可视化状态通过 setData()/data() 存储在每个 QGraphicsItem 上。
 *
 * 原因：
 *  - 避免对 NodeItem/EdgeItem 的类定义做侵入式修改。
 *  - 保持算法与界面解耦：算法层只产出 Step 序列。
 *  - 允许随时调用 resetStyle() 进行确定性重绘。
 */
constexpr int kRoleSccId     = 0x100;   // int：所属 SCC id，0 表示未分配
constexpr int kRoleInStack   = 0x101;   // bool：Tarjan 栈成员标记（SCC 阶段）
constexpr int kRoleActive    = 0x102;   // bool：当前 Step 的瞬时高亮

// --- 拓扑排序（Kahn）相关的持久化 界面 状态 ---
// 这些状态存到 item 的 data 里，而不是写进算法代码里，从而让 GraphView 仍然是可复用的
// 渲染组件（工程上常见的“关注点分离”）。
constexpr int kRoleTopoQueued = 0x110;  // bool：当前是否在 Kahn 队列中
constexpr int kRoleTopoDone   = 0x111;  // bool：是否已出队/输出
constexpr int kRoleTopoIndeg  = 0x112;  // int：当前入度值（用于刷新标签）

// --- 边的瞬时高亮（仅当前 Step） ---
constexpr int kRoleEdgeActive = 0x200;  // bool：当前 Step 高亮该边
constexpr int kRoleEdgeU      = 0x201;  // int：起点节点 id（缓存到 EdgeItem）
constexpr int kRoleEdgeV      = 0x202;  // int：终点节点 id（缓存到 EdgeItem）

static QColor sccColor(int sccId) {
    // 使用 HSV 生成确定性的鲜艳调色板；重绘/重新布局时颜色保持稳定。
    const int hue = (sccId * 47) % 360;
    return QColor::fromHsv(hue, 160, 255);
}

static QColor blendColor(const QColor& base, const QColor& overlay, double t)
{
    // 在 RGB 空间做线性插值，用于 界面 叠色足够稳定。
    // t ∈ [0,1]：0 表示 基色，1 表示 叠加色。
    auto lerp = [t](int a, int b) {
        return static_cast<int>(a + (b - a) * t);
    };
    return QColor(lerp(base.red(),   overlay.red()),
                  lerp(base.green(), overlay.green()),
                  lerp(base.blue(),  overlay.blue()));
}

static void styleNode(NodeItem* node) {
    if (!node) return;

    const int  sccId    = node->data(kRoleSccId).toInt();
    const bool inStack  = node->data(kRoleInStack).toBool();
    const bool active   = node->data(kRoleActive).toBool();

    // 拓扑排序状态（仅在 DAG 模式/Topo 回放阶段使用）。
    const bool queued   = node->data(kRoleTopoQueued).toBool();
    const bool done     = node->data(kRoleTopoDone).toBool();

    // ---------- 填充色（持久状态） ----------
    QColor baseFill = (sccId > 0) ? sccColor(sccId) : QColor(Qt::white);

    // Topo 回放时做轻微叠色，但保留 SCC 颜色的可辨识度。
    // done（已输出）需要与 queued（已入队/就绪）有明显区分。
    if (done)  baseFill = blendColor(baseFill, QColor(120, 200, 120), 0.35);
    if (queued) baseFill = blendColor(baseFill, QColor(100, 170, 255), 0.25);

    node->setBrush(QBrush(baseFill));

    // ---------- 描边（瞬时优先级） ----------
    // 优先级：active(红) > inStack(橙) > done(绿) > queued(蓝) > 默认(黑)
    QPen pen(Qt::black, 2);
    if (active) {
        pen = QPen(QColor(220, 40, 40), 4);
    } else if (inStack) {
        pen = QPen(QColor(255, 140, 0), 3);
    } else if (done) {
        pen = QPen(QColor(20, 140, 60), 3);
    } else if (queued) {
        pen = QPen(QColor(60, 120, 220), 3);
    }
    node->setPen(pen);
}

} // namespace

GraphView::GraphView(QWidget* parent)
    : QGraphicsView(parent)
{
    mScene = new QGraphicsScene(this);
    setScene(mScene);
    connect(&mForceTimer, &QTimer::timeout, this, &GraphView::onForceTick);
    mForceTimer.setInterval(16); // 约 60FPS
    if (mForceEnabled) mForceTimer.start();
    setRenderHint(QPainter::Antialiasing, true);
    // Qt 有时候窗口 resize 或滚轮缩放会让视图锚点变化，导致看起来缩得很奇怪，这两行能让缩放/resize 行为更稳定。
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);

}

void GraphView::showGraph(const Graph& g, const QVector<QPointF>& pos)
{
    // 保留旧接口，兼容现有调用方。
    showGraphEx(g, pos, QStringList(), QVector<int>());

}

void GraphView::showGraphEx(const Graph& g,
                            const QVector<QPointF>& pos,
                            const QStringList& labels,
                            const QVector<int>& colorId)
{
    // 这里刻意选择“重建 场景”：
    //  - 避免在模式切换（原图 vs DAG）时做复杂的增量 diff。
    //  - 保证每次阶段切换后的状态是确定且可复现的。
    mScene->clear();
    nodeItem.clear();
    edgeItem.clear();
    mEdgeWeight.clear();
    mIndegText.clear();
    mOrderText.clear();

    const qreal R = 30.0;
    mNodeRadius = R;

    // 1) 先创建节点（边需要拿到 NodeItem 指针）。
    for (int i = 1; i <= g.n; i++) {
        if (i <= 0 || i >= pos.size()) continue;

        auto* node = new NodeItem(i, R);
        node->setPen(QPen(Qt::black, 2));
        node->setPos(pos[i]);

        // 初始化每个节点的可视化状态（role 数据）。
        const int cid = (i < colorId.size()) ? colorId[i] : 0;
        node->setData(kRoleSccId, cid);
        node->setData(kRoleInStack, false);
        node->setData(kRoleActive, false);

        // Topo 相关 界面 状态始终初始化（即使本阶段暂时不用），
        // 这样后续算法切换不会依赖“之前显示过什么”。
        node->setData(kRoleTopoQueued, false);
        node->setData(kRoleTopoDone, false);
        node->setData(kRoleTopoIndeg, 0);

        // 标签作为子 item，跟随节点移动。
        const QString text = (i < labels.size() && !labels[i].isEmpty())
                                 ? labels[i]
                                 : QString::number(i);
        auto* label = new QGraphicsSimpleTextItem(text, node);
        QRectF br = label->boundingRect();
        label->setPos(-br.width() / 2.0, -br.height() / 2.0);

        mScene->addItem(node);
        nodeItem[i] = node;

        // 用户交互后重新“加热”力导布局，让布局继续调整。
        connect(node, &NodeItem::dragStarted, this, [this]() { heatUp(1.0); });
        connect(node, &NodeItem::dragEnded,   this, [this]() { heatUp(0.6); });
        connect(node, &NodeItem::pinChanged,  this, [this](bool) { heatUp(0.8); });
    }

    // 2) 再创建边。
    for (auto [u, v] : g.edges) {
        if (!nodeItem.contains(u) || !nodeItem.contains(v)) continue;
        auto* e = new EdgeItem(nodeItem[u], nodeItem[v], R);
        mScene->addItem(e);
        edgeItem[{u, v}] = e;

        // 在 EdgeItem 上缓存 (u,v)，以便样式/高亮无需额外外部映射。
        e->setData(kRoleEdgeU, u);
        e->setData(kRoleEdgeV, v);
        e->setData(kRoleEdgeActive, false);

        mEdgeWeight[{u, v}] = 1.0; // 旧边初始强度=1（完全生效）。
    }

    // 3) 重新渲染所有 item（例如 SCC 调色板填充色）。
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

    // 重置仿真参数，让重建后的图能从干净状态“稳定下来”。
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
        // QMap::operator[] 不是 const 安全的；在 const 上下文用 value() 查询。
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
    // 基于 item data 中存储的状态，确定性地重绘所有节点/边。
    // 每次应用 Step 后都会调用该函数，从而保证回放过程是确定性的。

    for (auto it = nodeItem.begin(); it != nodeItem.end(); ++it) {
        styleNode(it.value());
    }

    // 边的样式策略：
    //  - 活跃边：红色加粗
    //  - 从“已输出节点”出发的边：淡化（帮助观察 Kahn 的 frontier）
    for (auto it = edgeItem.begin(); it != edgeItem.end(); ++it) {
        const int u = it.key().first;
        EdgeItem* e = it.value();
        if (!e) continue;

        const bool active = e->data(kRoleEdgeActive).toBool();
        const bool fromDone = nodeItem.contains(u) ? nodeItem[u]->data(kRoleTopoDone).toBool() : false;

        QPen pen(Qt::black, 2);
        if (active) {
            pen = QPen(QColor(220, 40, 40), 4);
        } else if (fromDone) {
            pen = QPen(QColor(160, 160, 160), 2);
        }
        e->setPen(pen);
    }
}


void GraphView::applyStep(const Step& step)
{
    // 将单个算法 Step 应用到场景（只改状态，不直接画）。
    //
    // 设计要点（工程化理由）：
    //  - 算法层保持纯净且与 界面 解耦：只负责产出 Steps。
    //  - GraphView 将可视化状态存放在 QGraphicsItem 的 setData()/data() 中。
    //  - 每个 Step 只修改少量 role；随后 resetStyle() 做确定性重渲染。

    // 1) 清除上一帧的瞬时高亮（节点 + 边）。
    for (auto it = nodeItem.begin(); it != nodeItem.end(); ++it) {
        it.value()->setData(kRoleActive, false);
    }
    for (auto it = edgeItem.begin(); it != edgeItem.end(); ++it) {
        if (it.value()) it.value()->setData(kRoleEdgeActive, false);
    }

    // 2) 特殊处理：重置可视化状态。
    if (step.type == StepType::ResetVisual) {
        const bool clearScc = (step.val != 0);

        // 清理拓扑相关文本（入度/输出序号）。
        for (auto it = mIndegText.begin(); it != mIndegText.end(); ++it) {
            delete it.value();
        }
        mIndegText.clear();

        for (auto it = mOrderText.begin(); it != mOrderText.end(); ++it) {
            delete it.value();
        }
        mOrderText.clear();
        mTopoOrderIndex = 0;

        for (auto it = nodeItem.begin(); it != nodeItem.end(); ++it) {
            NodeItem* node = it.value();
            if (!node) continue;

            node->setData(kRoleActive, false);
            node->setData(kRoleInStack, false);

            // Topo 的持久化状态。
            node->setData(kRoleTopoQueued, false);
            node->setData(kRoleTopoDone, false);
            node->setData(kRoleTopoIndeg, 0);

            if (clearScc) node->setData(kRoleSccId, 0);
        }

        for (auto it = edgeItem.begin(); it != edgeItem.end(); ++it) {
            if (it.value()) it.value()->setData(kRoleEdgeActive, false);
        }

        resetStyle();
        return;
    }

    NodeItem* nodeU = nodeItem.contains(step.u) ? nodeItem[step.u] : nullptr;
    NodeItem* nodeV = nodeItem.contains(step.v) ? nodeItem[step.v] : nullptr;
    EdgeItem* edgeUV = edgeItem.contains({step.u, step.v}) ? edgeItem[{step.u, step.v}] : nullptr;

    auto ensureIndegText = [this](int id, NodeItem* node, int indeg) {
        if (!node) return;
        QGraphicsSimpleTextItem* t = mIndegText.value(id, nullptr);
        if (!t) {
            t = new QGraphicsSimpleTextItem(QString::number(indeg), node);
            t->setZValue(2);
            mIndegText[id] = t;
        } else {
            t->setText(QString::number(indeg));
        }
        QRectF br = t->boundingRect();
        // 将入度文字放在节点下方并居中。
        t->setPos(-br.width() / 2.0, mNodeRadius * 0.55);
    };

    auto ensureOrderText = [this](int id, NodeItem* node, int orderIdx) {
        if (!node) return;
        QGraphicsSimpleTextItem* t = mOrderText.value(id, nullptr);
        if (!t) {
            t = new QGraphicsSimpleTextItem(QString::number(orderIdx), node);
            t->setZValue(2);
            mOrderText[id] = t;
        } else {
            t->setText(QString::number(orderIdx));
        }
        QRectF br = t->boundingRect();
        // 将输出序号放在节点上方并居中。
        t->setPos(-br.width() / 2.0, -mNodeRadius - br.height() * 0.2);
    };

    switch (step.type) {
    // --- SCC（Tarjan）阶段 ---
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

    // --- 拓扑排序（Kahn）阶段 ---
    case StepType::TopoInitIndeg:
        if (nodeU) {
            nodeU->setData(kRoleTopoIndeg, step.val);
            ensureIndegText(step.u, nodeU, step.val);
            nodeU->setData(kRoleActive, true);
        }
        break;

    case StepType::TopoEnqueue:
        if (nodeU) {
            nodeU->setData(kRoleTopoQueued, true);
            nodeU->setData(kRoleActive, true);
        }
        break;

    case StepType::TopoDequeue:
        if (nodeU) {
            nodeU->setData(kRoleTopoQueued, false);
            nodeU->setData(kRoleTopoDone, true);
            nodeU->setData(kRoleActive, true);

            // 给节点分配输出序号（从 1 开始）。
            mTopoOrderIndex++;
            ensureOrderText(step.u, nodeU, mTopoOrderIndex);
        }
        break;

    case StepType::TopoIndegDec:
        // 高亮当前处理的边 (u -> v)，并更新 v 的入度显示。
        if (edgeUV) edgeUV->setData(kRoleEdgeActive, true);
        if (nodeV) {
            nodeV->setData(kRoleTopoIndeg, step.val);
            ensureIndegText(step.v, nodeV, step.val);
            nodeV->setData(kRoleActive, true);
        }
        if (nodeU) nodeU->setData(kRoleActive, true);
        break;

    default:
        // 本里程碑暂不处理；保留为空操作，便于后续扩展并保持向前兼容。
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

    // 冷却系数
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

            // 碰撞：dist < 2R 时额外弹开
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

    e->setData(kRoleEdgeU, u);
    e->setData(kRoleEdgeV, v);
    e->setData(kRoleEdgeActive, false);

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
