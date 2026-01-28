#pragma once

// Qt includes（必须有这些，否则 QGraphicsView / Q_OBJECT / QVector 等都找不到）
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsEllipseItem>
#include <QGraphicsTextItem>
#include <QMap>
#include <QVector>
#include <QPointF>

#include <QGraphicsPathItem>
#include <QGraphicsSimpleTextItem>
#include <cmath>
#include <QTimer>
#include <QGraphicsSceneMouseEvent>
#include <algorithm>
#include <QGraphicsRectItem>

#include "Graph.h"
#include "Steps.h"

// 前向声明
class NodeItem;
class EdgeItem;

class GraphView : public QGraphicsView {
    Q_OBJECT
public:
    explicit GraphView(QWidget* parent = nullptr);

    void showGraph(const Graph& g, const QVector<QPointF>& pos); // pos[1..n]
    void resetStyle();
    void applyStep(const Step& step);
    bool addEdge(int u, int v);          // 动态加边（只改视图）
    void setEdgeEditMode(bool on) { mEdgeEditMode = on; } // 可选：面板勾选后不需按Shift
signals:
    void edgeRequested(int u, int v);
public slots:
    void setForceEnabled(bool on);
    void startForceLayout();
    void stopForceLayout();
protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
private slots:
    void onForceTick();
private:
    QGraphicsScene* mScene = nullptr;
    QMap<int, NodeItem*> nodeItem;
    QMap<QPair<int,int>, class EdgeItem*> edgeItem;

    QRectF lastRect;

    QTimer mForceTimer;
    bool mForceEnabled = true;

    // Force 参数（默认值先用这套，之后你可以做成 UI 可调）
    // 如果你觉得“抖/乱飞”：减小 mDt 或减小 mRepulsion；如果“挤在一起”：增大 mRepulsion 或 mCollisionK。
    double mDt = 0.08;
    double mDamping = 0.85;       // 阻尼：越小越快停
    double mRepulsion = 120000.0;  // 点-点排斥强度
    double mSpringK = 0.08;       // 边弹簧强度
    double mRestLen = 120.0;      // 理想边长
    double mCenterPull = 0.002;  // 向中心的轻微拉力（避免跑飞）

    QRectF mLayoutBounds;

    // --- Force cooling / stability ---
    double mAlpha = 1.0;
    double mAlphaDecay = 0.03;   // 越大越快停
    double mAlphaMin = 0.01;     // 小于它就停表
    double mMaxSpeed = 20.0;     // 每 tick 最大位移速度
    double mCollisionK = 1.2;    // 防重叠力度
    qreal  mNodeRadius = 30.0;

    void heatUp(double a = 1.0) {
        mAlpha = std::max(mAlpha, a);
        if (mForceEnabled && !mForceTimer.isActive()) mForceTimer.start();
    }
    bool mEdgeEditMode = false;          // true=一直处于加边模式；false=按Shift才加边
    int mEdgeFrom = -1;
    NodeItem* mEdgeFromNode = nullptr;
    QGraphicsLineItem* mPreviewLine = nullptr;

    QMap<QPair<int,int>, double> mEdgeWeight; // 新边弹簧权重 0..1（更顺滑）

    QGraphicsRectItem* mArenaItem = nullptr;
    int mNodeCountHint = 0;

    void updateArena(int n);
    void clampNodeToArena(NodeItem* n);

    // --- Step playback visuals ---
    int mActiveNode = -1;
    QPair<int,int> mActiveEdge = {-1, -1};
    int mTopoOrderIndex = 0;

    QMap<int, QBrush> mBaseBrush;
    QMap<int, QPen>   mBasePen;
    QMap<QPair<int,int>, QPen> mBaseEdgePen;

    QMap<int, QGraphicsSimpleTextItem*> mIndegText;   // 入度显示
    QMap<int, QGraphicsSimpleTextItem*> mOrderText;   // 拓扑输出序号显示
};

class NodeItem : public QObject, public QGraphicsEllipseItem {
    Q_OBJECT
public:
    NodeItem(int id, qreal r)
        : m_id(id), m_r(r) {
        setRect(-r, -r, 2*r, 2*r);
        setFlag(ItemIsMovable, true);
        setFlag(ItemSendsGeometryChanges, true);
        setFlag(ItemIsSelectable, true);
    }

    int id() const { return m_id; }
    bool fixed() const { return m_fixed; }
    void setFixed(bool f) { m_fixed = f; }

    bool pinned() const { return m_pinned; }

    QPointF vel{0, 0};

signals:
    void moved();
    void dragStarted();
    void dragEnded();
    void pinChanged(bool pinned);

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override {
        if (change == ItemPositionHasChanged) emit moved();
        return QGraphicsEllipseItem::itemChange(change, value);
    }

    void mousePressEvent(QGraphicsSceneMouseEvent *e) override {
        emit dragStarted();
        QGraphicsEllipseItem::mousePressEvent(e);
    }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent *e) override {
        emit dragEnded();
        QGraphicsEllipseItem::mouseReleaseEvent(e);
    }

    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *e) override {
        m_pinned = !m_pinned;
        m_fixed = m_pinned;
        emit pinChanged(m_pinned);
        QGraphicsEllipseItem::mouseDoubleClickEvent(e);
    }

private:
    int m_id;
    qreal m_r;
    bool m_fixed = false;   // 真正的“固定”
    bool m_pinned = false;  // 双击切换 pin
};


class EdgeItem : public QObject, public QGraphicsPathItem {
    Q_OBJECT
public:
    EdgeItem(NodeItem* from, NodeItem* to, qreal nodeRadius)
        : m_from(from), m_to(to), m_r(nodeRadius)
    {
        setPen(QPen(Qt::black, 2));
        setZValue(-1); // 让边在点的下面

        connect(m_from, &NodeItem::moved, this, &EdgeItem::updatePath);
        connect(m_to,   &NodeItem::moved, this, &EdgeItem::updatePath);

        updatePath();
    }

public slots:
    void updatePath() {
        if (!m_from || !m_to) return;

        QPointF A = m_from->pos();
        QPointF B = m_to->pos();

        QLineF line(A, B);
        if (line.length() < 1.0) return;

        // 把线段两端“缩短”，避免穿过圆心（看起来更像连到圆边）
        QLineF ab(A, B);
        ab.setLength(std::max(0.0, ab.length() - m_r));
        QPointF end = ab.p2();

        QLineF ba(B, A);
        ba.setLength(std::max(0.0, ba.length() - m_r));
        QPointF start = ba.p2();

        QLineF core(start, end);

        QPainterPath path;
        path.moveTo(core.p1());
        path.lineTo(core.p2());

        // 箭头（三角形两翼）
        const qreal arrowSize = 10.0;
        const qreal angle = std::atan2(-core.dy(), core.dx());

        QPointF p1 = core.p2() + QPointF(std::cos(angle + M_PI * 2.0/3.0) * arrowSize,
                                         -std::sin(angle + M_PI * 2.0/3.0) * arrowSize);
        QPointF p2 = core.p2() + QPointF(std::cos(angle + M_PI * 4.0/3.0) * arrowSize,
                                         -std::sin(angle + M_PI * 4.0/3.0) * arrowSize);

        path.moveTo(p1);
        path.lineTo(core.p2());
        path.lineTo(p2);

        setPath(path);
    }

private:
    NodeItem* m_from = nullptr;
    NodeItem* m_to   = nullptr;
    qreal m_r = 0;
};
