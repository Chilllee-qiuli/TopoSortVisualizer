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
    edgeItem.clear();

    const qreal R = 30.0;

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
