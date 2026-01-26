#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QVBoxLayout>
#include <QLabel>
#include <cmath>
#include <QRegularExpression>

static QVector<QPointF> makeCirclePos(int n, double radius = 250.0)
{
    QVector<QPointF> pos(n + 1);
    for (int i = 1; i <= n; ++i) {
        double ang = 2.0 * M_PI * (i - 1) / n;
        pos[i] = QPointF(radius * std::cos(ang), radius * std::sin(ang));
    }
    return pos;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    view = new GraphView(this);
    setCentralWidget(view);

    // 右侧控制面板
    auto* dock = new QDockWidget("控制面板", this);
    auto* panel = new QWidget(dock);
    auto* lay = new QVBoxLayout(panel);

    lay->addWidget(new QLabel("节点数量 (n):"));
    nSpin = new QSpinBox(panel);
    nSpin->setRange(1, 200);
    nSpin->setValue(6);
    lay->addWidget(nSpin);

    auto* btnCreate = new QPushButton("建图！", panel);
    lay->addWidget(btnCreate);

    lay->addSpacing(10);
    lay->addWidget(new QLabel("再加一条边 (u -> v):"));

    uSpin = new QSpinBox(panel);
    vSpin = new QSpinBox(panel);
    uSpin->setRange(1, 6);
    vSpin->setRange(1, 6);
    lay->addWidget(uSpin);
    lay->addWidget(vSpin);

    auto* btnAddEdge = new QPushButton("加边", panel);
    lay->addWidget(btnAddEdge);

    lay->addSpacing(10);
    lay->addWidget(new QLabel("批处理边 (each line: u v):"));
    edgesEdit = new QTextEdit(panel);
    edgesEdit->setPlaceholderText("Example:\n1 2\n2 3\n1 3");
    lay->addWidget(edgesEdit);

    auto* btnAddBatch = new QPushButton("从文本添加边", panel);
    lay->addWidget(btnAddBatch);

    lay->addSpacing(10);
    lay->addWidget(new QLabel("提示：按住Shift键，从节点点击到目标节点以添加边。"));
    lay->addStretch(1);

    panel->setLayout(lay);
    dock->setWidget(panel);
    addDockWidget(Qt::RightDockWidgetArea, dock);

    connect(btnCreate, &QPushButton::clicked, this, &MainWindow::onCreateGraph);
    connect(btnAddEdge, &QPushButton::clicked, this, &MainWindow::onAddEdge);
    connect(btnAddBatch, &QPushButton::clicked, this, &MainWindow::onAddEdgesFromText);

    connect(view, &GraphView::edgeRequested, this, &MainWindow::onEdgeRequested);

    // 默认先创建一个
    onCreateGraph();
}

bool MainWindow::addEdgeImpl(int u, int v)
{
    if (mGraph.n <= 0) return false;
    if (u < 1 || u > mGraph.n || v < 1 || v > mGraph.n) return false;
    if (u == v) return false;

    // 去重（简单扫一遍就够用）
    for (auto &e : mGraph.edges) {
        if (e.first == u && e.second == v) return false;
    }

    mGraph.addEdge(u, v);
    view->addEdge(u, v);
    return true;
}

void MainWindow::onCreateGraph()
{
    int n = nSpin->value();
    mGraph = Graph(n);
    mPos = makeCirclePos(n);

    uSpin->setRange(1, n);
    vSpin->setRange(1, n);

    view->showGraph(mGraph, mPos);
}

void MainWindow::onAddEdge()
{
    addEdgeImpl(uSpin->value(), vSpin->value());
}

void MainWindow::onAddEdgesFromText()
{
    QString text = edgesEdit->toPlainText();
    auto lines = text.split('\n', Qt::SkipEmptyParts);

    int ok = 0;
    for (auto &line : lines) {
        auto parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);

        if (parts.size() < 2) continue;
        bool a=false,b=false;
        int u = parts[0].toInt(&a);
        int v = parts[1].toInt(&b);
        if (a && b && addEdgeImpl(u, v)) ok++;
    }
    statusBar()->showMessage(QString("Added %1 edges").arg(ok), 2000);
}

void MainWindow::onEdgeRequested(int u, int v)
{
    addEdgeImpl(u, v);
}
MainWindow::~MainWindow()
{
    delete ui;
}
