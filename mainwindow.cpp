#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "GraphView.h"
#include "Graph.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    //  创建 GraphView，并把它作为窗口中央部件
    view = new GraphView(this);
    setCentralWidget(view);

    //  画一个最小 demo 图（验证不是灰屏）
    Graph g(4);
    g.addEdge(1, 2);
    g.addEdge(2, 3);
    g.addEdge(3, 4);
    g.addEdge(1, 3);

    QVector<QPointF> pos(5); // 下标从 1 到 n
    pos[1] = QPointF(0, 0);
    pos[2] = QPointF(200, 0);
    pos[3] = QPointF(200, 200);
    pos[4] = QPointF(0, 200);

    view->showGraph(g, pos);
}

MainWindow::~MainWindow()
{
    delete ui;
}

