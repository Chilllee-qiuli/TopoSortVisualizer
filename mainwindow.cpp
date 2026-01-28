#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "TarjanSCC.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QMenu>
#include <QKeySequence>
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



void MainWindow::setupPanelsMenu()
{
    // A dock can be closed via the [x] button, which only hides it (unless WA_DeleteOnClose is set).
    // Provide menu toggles so the user can always bring panels back.
    QMenu* panelsMenu = menuBar()->addMenu(tr("Panels"));
    mGraphDockAction = mGraphDock->toggleViewAction();
    mAlgoDockAction  = mAlgoDock->toggleViewAction();

    // Optional ergonomic shortcuts.
    mGraphDockAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_1));
    mAlgoDockAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_2));

    panelsMenu->addAction(mGraphDockAction);
    panelsMenu->addAction(mAlgoDockAction);
}

void MainWindow::updateEdgeCountUI()
{
    if (!edgeCountLabel) return;
    const int m = static_cast<int>(mGraph.edges.size());
    edgeCountLabel->setText(QString("Edges: %1").arg(m));
}

void MainWindow::setupDocks()
{
    // ----------------------
    // Dock 1: Graph builder
    // ----------------------
    mGraphDock = new QDockWidget(tr("Graph Builder"), this);
    mGraphDock->setObjectName("GraphBuilderDock");

    auto* graphPanel = new QWidget(mGraphDock);
    auto* gLay = new QVBoxLayout(graphPanel);

    edgeCountLabel = new QLabel(graphPanel);
    edgeCountLabel->setText("Edges: 0");
    gLay->addWidget(edgeCountLabel);

    gLay->addWidget(new QLabel(tr("Nodes (n):")));
    nSpin = new QSpinBox(graphPanel);
    nSpin->setRange(1, 200);
    nSpin->setValue(6);
    gLay->addWidget(nSpin);

    auto* btnCreate = new QPushButton(tr("Create Graph"), graphPanel);
    gLay->addWidget(btnCreate);

    gLay->addSpacing(10);
    gLay->addWidget(new QLabel(tr("Add one edge (u -> v):")));

    uSpin = new QSpinBox(graphPanel);
    vSpin = new QSpinBox(graphPanel);
    uSpin->setRange(1, 6);
    vSpin->setRange(1, 6);
    gLay->addWidget(uSpin);
    gLay->addWidget(vSpin);

    auto* btnAddEdge = new QPushButton(tr("Add Edge"), graphPanel);
    gLay->addWidget(btnAddEdge);

    gLay->addSpacing(10);
    gLay->addWidget(new QLabel(tr("Batch edges (each line: u v):")));
    edgesEdit = new QTextEdit(graphPanel);
    edgesEdit->setPlaceholderText("Example:\n1 2\n2 3\n1 3");
    gLay->addWidget(edgesEdit);

    auto* btnAddBatch = new QPushButton(tr("Add From Text"), graphPanel);
    gLay->addWidget(btnAddBatch);

    gLay->addSpacing(10);
    gLay->addWidget(new QLabel(tr("Tip: Hold Shift and drag from a node to another to add an edge.")));

    gLay->addStretch(1);
    graphPanel->setLayout(gLay);
    mGraphDock->setWidget(graphPanel);
    addDockWidget(Qt::RightDockWidgetArea, mGraphDock);

    // Connect builder signals
    connect(btnCreate, &QPushButton::clicked, this, &MainWindow::onCreateGraph);
    connect(btnAddEdge, &QPushButton::clicked, this, &MainWindow::onAddEdge);
    connect(btnAddBatch, &QPushButton::clicked, this, &MainWindow::onAddEdgesFromText);

    // ----------------------
    // Dock 2: Algorithms
    // ----------------------
    mAlgoDock = new QDockWidget(tr("Algorithms"), this);
    mAlgoDock->setObjectName("AlgoDock");

    auto* algoPanel = new QWidget(mAlgoDock);
    auto* aLay = new QVBoxLayout(algoPanel);

    aLay->addWidget(new QLabel(tr("Tarjan SCC Visualization")));

    runSccBtn = new QPushButton(tr("Run SCC (Tarjan)"), algoPanel);
    aLay->addWidget(runSccBtn);

    playBtn = new QPushButton(tr("Play"), algoPanel);
    playBtn->setEnabled(false);
    aLay->addWidget(playBtn);

    nextBtn = new QPushButton(tr("Step"), algoPanel);
    nextBtn->setEnabled(false);
    aLay->addWidget(nextBtn);

    resetAlgoBtn = new QPushButton(tr("Reset"), algoPanel);
    resetAlgoBtn->setEnabled(false);
    aLay->addWidget(resetAlgoBtn);

    aLay->addWidget(new QLabel(tr("Step log:")));
    logEdit = new QTextEdit(algoPanel);
    logEdit->setReadOnly(true);
    logEdit->setMinimumHeight(140);
    aLay->addWidget(logEdit);

    aLay->addStretch(1);

    algoPanel->setLayout(aLay);
    mAlgoDock->setWidget(algoPanel);

    // Place below builder dock to reduce visual clutter.
    addDockWidget(Qt::RightDockWidgetArea, mAlgoDock);
    splitDockWidget(mGraphDock, mAlgoDock, Qt::Vertical);

    // Connect algorithm signals
    connect(runSccBtn, &QPushButton::clicked, this, &MainWindow::onRunSCC);
    connect(playBtn, &QPushButton::clicked, this, &MainWindow::onPlayPause);
    connect(nextBtn, &QPushButton::clicked, this, &MainWindow::onNextStep);
    connect(resetAlgoBtn, &QPushButton::clicked, this, &MainWindow::onResetAlgo);

    // Menu toggles so docks can be reopened after being closed.
    setupPanelsMenu();
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    view = new GraphView(this);
    setCentralWidget(view);
// Panels (Graph Builder + Algorithms) are implemented as QDockWidgets.
// Users can close them; we provide menu toggles to re-open them at any time.
setupDocks();

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
    updateEdgeCountUI();
    
    // Graph mutated -> invalidate cached algorithm steps and clear SCC state.
    // This avoids displaying stale SCC colors/steps on a changed graph.
    if (view) view->applyStep({StepType::ResetVisual, -1, -1, -1, 1, QString()});
    onResetAlgo();
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
    updateEdgeCountUI();

    // Reset algorithm playback when graph changes.
    onResetAlgo();
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
    statusBar()->showMessage(QString("Added %1 edges (total %2)").arg(ok).arg(mGraph.edges.size()), 2000);
}

void MainWindow::onEdgeRequested(int u, int v)
{
    addEdgeImpl(u, v);
}

void MainWindow::onRunSCC()
{

    // Ensure we start from a clean visualization state so SCC colors appear progressively.
    if (view) view->applyStep({StepType::ResetVisual, -1, -1, -1, 1, QString("Reset visual state")});
    // Run Tarjan SCC on the current directed graph and cache steps for playback.
    // The algorithm itself is pure; visualization happens in GraphView::applyStep().
    TarjanSCC tarjan;
    SCCResult res = tarjan.run(mGraph);

    mSteps.clear();
    mSteps.reserve((int)res.steps.size());
    for (const Step& s : res.steps) mSteps.push_back(s);
    mStepIndex = 0;

    if (logEdit) {
        logEdit->clear();
        logEdit->append(QString("SCC count = %1").arg(res.sccCnt));
        logEdit->append("----");
    }

    // Enable playback controls.
    playBtn->setEnabled(!mSteps.isEmpty());
    nextBtn->setEnabled(!mSteps.isEmpty());
    resetAlgoBtn->setEnabled(true);

    statusBar()->showMessage(QString("Tarjan SCC produced %1 steps").arg(mSteps.size()), 2500);
}

void MainWindow::onPlayPause()
{
    if (mSteps.isEmpty()) return;

    mPlaying = !mPlaying;
    playBtn->setText(mPlaying ? "暂停" : "播放");

    if (mPlaying) mPlayTimer.start();
    else mPlayTimer.stop();
}

void MainWindow::onNextStep()
{
    if (mSteps.isEmpty()) return;
    if (mStepIndex >= mSteps.size()) return;

    const Step& st = mSteps[mStepIndex++];
    view->applyStep(st);

    if (logEdit && !st.note.isEmpty()) logEdit->append(st.note);

    // End condition.
    if (mStepIndex >= mSteps.size()) {
        mPlaying = false;
        mPlayTimer.stop();
        playBtn->setText("播放");
        statusBar()->showMessage("SCC steps finished", 2000);
    }
}

void MainWindow::onPlayTick()
{
    if (!mPlaying) return;
    onNextStep();
}

void MainWindow::onResetAlgo()
{
    // Stop playback and clear transient highlight, but keep the current graph.
    mPlayTimer.stop();
    mPlaying = false;
    if (playBtn) playBtn->setText("播放");

    mSteps.clear();
    mStepIndex = 0;

    if (logEdit) logEdit->clear();
    if (view) view->applyStep({StepType::ResetVisual, -1, -1, -1, 0, QString()});

    if (playBtn) playBtn->setEnabled(false);
    if (nextBtn) nextBtn->setEnabled(false);
    if (resetAlgoBtn) resetAlgoBtn->setEnabled(false);
}

MainWindow::~MainWindow()
{
    delete ui;
}
