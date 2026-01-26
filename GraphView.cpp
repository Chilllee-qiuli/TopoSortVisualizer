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
    setRenderHint(QPainter::Antialiasing, true);
    // Qt 有时候窗口 resize 或滚轮缩放会让视图锚点变化，导致看起来缩得很奇怪，这两行能让缩放/resize 行为更稳定。
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);

}

void GraphView::showGraph(const Graph& g, const QVector<QPointF>& pos)
{
    mScene->clear();
    nodeItem.clear();
    nodeText.clear();

    // 1) 先画边（线）
    QPen edgePen(Qt::black);
    for (auto [u, v] : g.edges) {
        if (u <= 0 || v <= 0 || u >= pos.size() || v >= pos.size()) continue;
        mScene->addLine(QLineF(pos[u], pos[v]), edgePen);
    }

    // 2) 再画点（圆 + 文本）
    const double R = 30.0;
    QPen nodePen(Qt::black);
    QBrush nodeBrush(Qt::white);

    for (int i = 1; i <= g.n; i++) {
        QPointF p = pos[i];
        auto* ellipse = mScene->addEllipse(p.x() - R, p.y() - R, 2*R, 2*R, nodePen, nodeBrush);
        auto* text = mScene->addText(QString::number(i));
        text->setPos(p.x() - 6, p.y() - 10);

        nodeItem[i] = ellipse;
        nodeText[i] = text;
    }

    QRectF rect = mScene->itemsBoundingRect();
    rect = rect.adjusted(-80, -80, 80, 80);
    mScene->setSceneRect(rect);

    // ✅ 关键 1：清掉之前可能很小的缩放变换
    resetTransform();

    // ✅ 关键 2：保存下来，窗口 resize 时还会再 fit 一次
    lastRect = rect;

    // ✅ 关键 3：延迟到界面布局完成后再 fit（避免 viewport 还是 0/很小）
    QTimer::singleShot(0, this, [this]() {
        if (!lastRect.isNull())
            fitInView(lastRect, Qt::KeepAspectRatio);
    });
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
