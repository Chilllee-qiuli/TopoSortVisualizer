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

private:
    QGraphicsScene* mScene = nullptr;
    QMap<int, NodeItem*> nodeItem;
    QMap<QPair<int,int>, class EdgeItem*> edgeItem;

    QRectF lastRect;
protected:
    void resizeEvent(QResizeEvent* event) override;
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

    QPointF vel{0,0};

signals:
    void moved();

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override {
        if (change == ItemPositionHasChanged) emit moved();
        return QGraphicsEllipseItem::itemChange(change, value);
    }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent *e) override {
        m_fixed = true; // 放开就固定（也可以改成点击切换）
        QGraphicsEllipseItem::mouseReleaseEvent(e);
    }

private:
    int m_id;
    qreal m_r;
    bool m_fixed = false;
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
