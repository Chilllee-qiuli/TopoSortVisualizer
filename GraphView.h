#pragma once

// Qt includes（必须有这些，否则 QGraphicsView / Q_OBJECT / QVector 等都找不到）
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsEllipseItem>
#include <QGraphicsTextItem>
#include <QMap>
#include <QVector>
#include <QPointF>

#include "Graph.h"
#include "Steps.h"

class GraphView : public QGraphicsView {
    Q_OBJECT
public:
    explicit GraphView(QWidget* parent = nullptr);

    void showGraph(const Graph& g, const QVector<QPointF>& pos); // pos[1..n]
    void resetStyle();
    void applyStep(const Step& step);

private:
    QGraphicsScene* mScene = nullptr;
    QMap<int, QGraphicsEllipseItem*> nodeItem;
    QMap<int, QGraphicsTextItem*> nodeText;
    QRectF lastRect;
protected:
    void resizeEvent(QResizeEvent* event) override;
};
