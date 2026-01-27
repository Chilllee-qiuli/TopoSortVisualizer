#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QActionGroup>
#include <cmath>
#include <QRegularExpression>

// 保留两套初始铺开的逻辑，分别用于点少于等于10和多余10
static QVector<QPointF> makeCirclePos(int n, double radius = 250.0)
{
    QVector<QPointF> pos(n + 1);
    for (int i = 1; i <= n; ++i) {
        double ang = 2.0 * M_PI * (i - 1) / n;
        pos[i] = QPointF(radius * std::cos(ang), radius * std::sin(ang));
    }
    return pos;
}
// 点多时 250 半径会非常挤，力导一开始就会“爆炸”推开（看起来像弹飞）。改为初始直接铺网格。
static QVector<QPointF> makeGridPos(int n, double gap = 90.0)
{
    QVector<QPointF> pos(n + 1);
    int cols = (int)std::ceil(std::sqrt((double)n));
    int rows = (int)std::ceil(n / (double)cols);

    double startX = -(cols - 1) * gap / 2.0;
    double startY = -(rows - 1) * gap / 2.0;

    for (int i = 1; i <= n; ++i) {
        int idx = i - 1;
        int r = idx / cols;
        int c = idx % cols;
        pos[i] = QPointF(startX + c * gap, startY + r * gap);
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

    // ==========================
    // 右侧 Dock：面板切换（建图/算法）
    // ==========================
    controlsDock = new QDockWidget("控制面板", this);
    auto* dockRoot = new QWidget(controlsDock);
    auto* dockRootLay = new QVBoxLayout(dockRoot);

    // 顶部：下拉选择当前面板
    auto* modeRow = new QHBoxLayout();
    modeRow->addWidget(new QLabel("面板：", dockRoot));
    panelModeCombo = new QComboBox(dockRoot);
    panelModeCombo->addItems({"建图", "算法"});
    modeRow->addWidget(panelModeCombo, 1);
    dockRootLay->addLayout(modeRow);

    // 堆叠页：建图页 / 算法页
    panelStack = new QStackedWidget(dockRoot);
    dockRootLay->addWidget(panelStack, 1);

    // ---------- 建图页 ----------
    auto* buildPage = new QWidget(panelStack);
    auto* buildLay = new QVBoxLayout(buildPage);

    buildLay->addWidget(new QLabel("节点数量 (n):"));
    nSpin = new QSpinBox(buildPage);
    nSpin->setRange(1, 200);
    nSpin->setValue(6);
    buildLay->addWidget(nSpin);

    auto* btnCreate = new QPushButton("建图！", buildPage);
    buildLay->addWidget(btnCreate);

    buildLay->addSpacing(10);
    buildLay->addWidget(new QLabel("再加一条边 (u -> v):"));

    uSpin = new QSpinBox(buildPage);
    vSpin = new QSpinBox(buildPage);
    uSpin->setRange(1, 6);
    vSpin->setRange(1, 6);
    buildLay->addWidget(uSpin);
    buildLay->addWidget(vSpin);

    auto* btnAddEdge = new QPushButton("加边", buildPage);
    buildLay->addWidget(btnAddEdge);

    buildLay->addSpacing(10);
    buildLay->addWidget(new QLabel("批处理边 (each line: u v):"));
    edgesEdit = new QTextEdit(buildPage);
    edgesEdit->setPlaceholderText("例如:\n1 2\n2 3\n1 3");
    buildLay->addWidget(edgesEdit);

    auto* btnAddBatch = new QPushButton("从文本添加边", buildPage);
    buildLay->addWidget(btnAddBatch);

    edgeCountLabel = new QLabel("当前边数: 0", buildPage);
    buildLay->addWidget(edgeCountLabel);

    buildLay->addSpacing(10);
    buildLay->addWidget(new QLabel("提示：按住Shift键，从节点点击到目标节点以添加边。"));

    buildLay->addStretch(1);

    // ---------- 算法页 ----------
    auto* algoPage = new QWidget(panelStack);
    auto* algoLay = new QVBoxLayout(algoPage);

    graphInfoAlgoLabel = new QLabel("Graph: n=-, edges=-", algoPage);
    algoLay->addWidget(graphInfoAlgoLabel);

    algoLay->addSpacing(15);
    algoLay->addWidget(new QLabel("Steps 播放："));

    btnRunScc = new QPushButton("Run Tarjan(SCC)", algoPage);
    btnRunTopo = new QPushButton("Run Topo(Kahn)", algoPage);
    btnNext = new QPushButton("Next Step", algoPage);
    btnPlay = new QPushButton("Play/Pause", algoPage);

    algoLay->addWidget(btnRunScc);
    algoLay->addWidget(btnRunTopo);
    algoLay->addWidget(btnNext);
    algoLay->addWidget(btnPlay);

    algoLay->addWidget(new QLabel("Speed(ms/step):"));
    speedSpin = new QSpinBox(algoPage);
    speedSpin->setRange(20, 2000);
    speedSpin->setValue(300);
    algoLay->addWidget(speedSpin);

    logEdit = new QTextEdit(algoPage);
    logEdit->setReadOnly(true);
    logEdit->setPlaceholderText("Step logs...");
    algoLay->addWidget(logEdit);

    algoLay->addStretch(1);

    // 加入堆叠
    panelStack->addWidget(buildPage); // index 0
    panelStack->addWidget(algoPage);  // index 1

    // 切换逻辑
    connect(panelModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            panelStack, &QStackedWidget::setCurrentIndex);
    panelModeCombo->setCurrentIndex(0);

    dockRoot->setLayout(dockRootLay);
    controlsDock->setWidget(dockRoot);
    addDockWidget(Qt::RightDockWidgetArea, controlsDock);

    // ==========================
    // 信号连接
    // ==========================
    connect(btnCreate, &QPushButton::clicked, this, &MainWindow::onCreateGraph);
    connect(btnAddEdge, &QPushButton::clicked, this, &MainWindow::onAddEdge);
    connect(btnAddBatch, &QPushButton::clicked, this, &MainWindow::onAddEdgesFromText);

    connect(view, &GraphView::edgeRequested, this, &MainWindow::onEdgeRequested);

    connect(btnRunScc, &QPushButton::clicked, this, &MainWindow::onRunSccDemo);
    connect(btnRunTopo, &QPushButton::clicked, this, &MainWindow::onRunTopoDemo);
    connect(btnNext, &QPushButton::clicked, this, &MainWindow::onNextStep);
    connect(btnPlay, &QPushButton::clicked, this, &MainWindow::onTogglePlay);

    connect(&mPlayTimer, &QTimer::timeout, this, &MainWindow::onNextStep);

    // 默认先创建一个
    onCreateGraph();

    // ==========================
    // View 菜单：选择“建图/算法面板” + 显示/隐藏 Dock
    // ==========================
    auto* viewMenu = menuBar()->addMenu(tr("View"));

    auto* actGroup = new QActionGroup(this);
    actGroup->setExclusive(true);

    auto* actBuildPanel = viewMenu->addAction(tr("Build Panel"));
    auto* actAlgoPanel  = viewMenu->addAction(tr("Algorithm Panel"));
    actBuildPanel->setCheckable(true);
    actAlgoPanel->setCheckable(true);
    actGroup->addAction(actBuildPanel);
    actGroup->addAction(actAlgoPanel);
    actBuildPanel->setChecked(true);

    connect(actBuildPanel, &QAction::triggered, this, [this]() {
        if (controlsDock) controlsDock->show();
        if (panelModeCombo) panelModeCombo->setCurrentIndex(0);
    });
    connect(actAlgoPanel, &QAction::triggered, this, [this]() {
        if (controlsDock) controlsDock->show();
        if (panelModeCombo) panelModeCombo->setCurrentIndex(1);
    });

    // combo 改了，也同步菜单勾选状态
    connect(panelModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [actBuildPanel, actAlgoPanel](int idx){
        if (idx == 0) actBuildPanel->setChecked(true);
        else         actAlgoPanel->setChecked(true);
    });

    viewMenu->addSeparator();
    viewMenu->addAction(controlsDock->toggleViewAction());
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

    updateEdgeCount();   // 谁加边，谁负责刷新统计
    return true;
}

void MainWindow::onCreateGraph()
{
    int n = nSpin->value();
    mGraph = Graph(n);
    if(n <= 10) mPos = makeCirclePos(n);
    else mPos = makeGridPos(n);

    uSpin->setRange(1, n);
    vSpin->setRange(1, n);

    view->showGraph(mGraph, mPos);

    updateEdgeCount();
}

void MainWindow::onAddEdge()
{
    addEdgeImpl(uSpin->value(), vSpin->value());
    updateEdgeCount();
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
    statusBar()->showMessage(QString("本次批量操作增加了 %1 条边").arg(ok), 2000);

    updateEdgeCount();
}

void MainWindow::updateEdgeCount()
{
    const int edges = (int)mGraph.edges.size();

    if (edgeCountLabel)
        edgeCountLabel->setText(QString("当前边数: %1").arg(edges));

    if (graphInfoAlgoLabel)
        graphInfoAlgoLabel->setText(QString("Graph: n=%1, edges=%2").arg(mGraph.n).arg(edges));
}

void MainWindow::onEdgeRequested(int u, int v)
{
    addEdgeImpl(u, v);
}

void MainWindow::loadSteps(const std::vector<Step>& steps)
{
    mSteps = QVector<Step>(steps.begin(), steps.end());
    mStepIdx = 0;

    view->resetStyle();
    if (logEdit) logEdit->clear();

    // ✅ 运行算法后自动切到“算法”面板，方便直接播放/看日志
    if (panelModeCombo) panelModeCombo->setCurrentIndex(1);

    statusBar()->showMessage(QString("Loaded %1 steps").arg(mSteps.size()), 1500);
}

void MainWindow::onRunSccDemo()
{
    // 重新画一下当前图（保证干净）
    view->showGraph(mGraph, mPos);

    TarjanSCC algo;
    auto res = algo.run(mGraph);
    loadSteps(res.steps);
}

void MainWindow::onRunTopoDemo()
{
    // 重新画一下当前图（Topo 演示先要求你输入的是 DAG；不是也能播，但 order 不一定完整）
    view->showGraph(mGraph, mPos);

    TopoKahn algo;
    auto res = algo.run(mGraph);
    loadSteps(res.steps);

    if (!res.ok) {
        statusBar()->showMessage("Warning: graph may have cycle, topo not complete.", 3000);
    }
}

void MainWindow::onNextStep()
{
    if (mStepIdx >= mSteps.size()) {
        mPlayTimer.stop();
        statusBar()->showMessage("Steps finished.", 1500);
        return;
    }

    const Step& s = mSteps[mStepIdx++];
    view->applyStep(s);

    if (logEdit) logEdit->append(s.note);
}

void MainWindow::onTogglePlay()
{
    if (mPlayTimer.isActive()) {
        mPlayTimer.stop();
        return;
    }
    mPlayTimer.start(speedSpin ? speedSpin->value() : 300);
}

MainWindow::~MainWindow()
{
    delete ui;
}
