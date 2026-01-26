#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "GraphView.h"
#include "Graph.h"
#include <QDockWidget>
#include <QSpinBox>
#include <QPushButton>
#include <QTextEdit>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;

    //保存一个指向 GraphView 的指针，后面便于调用 showGraph() 画图。
    GraphView* view = nullptr;

    void onCreateGraph();
    void onAddEdge();
    void onAddEdgesFromText();
    void onEdgeRequested(int u, int v);

private:
    Graph mGraph;
    QVector<QPointF> mPos;

    QSpinBox* nSpin = nullptr;
    QSpinBox* uSpin = nullptr;
    QSpinBox* vSpin = nullptr;
    QTextEdit* edgesEdit = nullptr;

    bool addEdgeImpl(int u, int v); // 统一入口

};
#endif // MAINWINDOW_H
