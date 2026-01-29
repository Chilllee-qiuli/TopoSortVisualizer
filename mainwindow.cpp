#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "TarjanSCC.h"
#include "Condense.h"
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
    edgeCountLabel->setText(QString("当前边数: %1").arg(m));
}

void MainWindow::setupDocks()
{
    // ----------------------
    // 控制面板1：建图
    // ----------------------
    mGraphDock = new QDockWidget(tr("建图"), this);
    mGraphDock->setObjectName("建图面板");

    auto* graphPanel = new QWidget(mGraphDock);
    auto* gLay = new QVBoxLayout(graphPanel);

    edgeCountLabel = new QLabel(graphPanel);
    edgeCountLabel->setText("边s: 0");
    gLay->addWidget(edgeCountLabel);

    gLay->addWidget(new QLabel(tr("节点数量 (n):")));
    nSpin = new QSpinBox(graphPanel);
    nSpin->setRange(1, 200);
    nSpin->setValue(6);
    gLay->addWidget(nSpin);

    auto* btnCreate = new QPushButton(tr("点击建图！"), graphPanel);
    gLay->addWidget(btnCreate);

    gLay->addSpacing(10);
    gLay->addWidget(new QLabel(tr("加一条边 (u -> v):")));

    uSpin = new QSpinBox(graphPanel);
    vSpin = new QSpinBox(graphPanel);
    uSpin->setRange(1, 6);
    vSpin->setRange(1, 6);
    gLay->addWidget(uSpin);
    gLay->addWidget(vSpin);

    auto* btnAddEdge = new QPushButton(tr("加边"), graphPanel);
    gLay->addWidget(btnAddEdge);

    gLay->addSpacing(10);
    gLay->addWidget(new QLabel(tr("批量加边 (u -> v):")));
    edgesEdit = new QTextEdit(graphPanel);
    edgesEdit->setPlaceholderText("例如:\n1 2\n2 3\n1 3");
    gLay->addWidget(edgesEdit);

    auto* btnAddBatch = new QPushButton(tr("从文本中加边"), graphPanel);
    gLay->addWidget(btnAddBatch);

    gLay->addSpacing(10);
    gLay->addWidget(new QLabel(tr("提示：按住shift同时点击可以在图中加边.")));

    gLay->addStretch(1);
    graphPanel->setLayout(gLay);
    mGraphDock->setWidget(graphPanel);
    addDockWidget(Qt::RightDockWidgetArea, mGraphDock);

    // Connect builder signals
    connect(btnCreate, &QPushButton::clicked, this, &MainWindow::onCreateGraph);
    connect(btnAddEdge, &QPushButton::clicked, this, &MainWindow::onAddEdge);
    connect(btnAddBatch, &QPushButton::clicked, this, &MainWindow::onAddEdgesFromText);

    // ----------------------
    // 面板2：算法
    // ----------------------
    mAlgoDock = new QDockWidget(tr("算法"), this);
    mAlgoDock->setObjectName("算法面板");

    auto* algoPanel = new QWidget(mAlgoDock);
    auto* aLay = new QVBoxLayout(algoPanel);

    aLay->addWidget(new QLabel(tr("Tarjan可视化缩点")));

    runSccBtn = new QPushButton(tr("回到开始缩点 (Tarjan)"), algoPanel);
    aLay->addWidget(runSccBtn);

    // **切换到 DAG 显示**：质心初始位置 + 再跑力导布局
    // These buttons are enabled after we have a valid SCC result.
    showDagBtn = new QPushButton(tr("展示 DAG (缩点结果)"), algoPanel);
    showDagBtn->setEnabled(false);
    aLay->addWidget(showDagBtn);

    showOriBtn = new QPushButton(tr("回溯"), algoPanel);
    showOriBtn->setEnabled(false);
    aLay->addWidget(showOriBtn);

    playBtn = new QPushButton(tr("执行"), algoPanel);
    playBtn->setEnabled(false);
    aLay->addWidget(playBtn);

    nextBtn = new QPushButton(tr("步骤"), algoPanel);
    nextBtn->setEnabled(false);
    aLay->addWidget(nextBtn);

    resetAlgoBtn = new QPushButton(tr("重置"), algoPanel);
    resetAlgoBtn->setEnabled(false);
    aLay->addWidget(resetAlgoBtn);

    aLay->addWidget(new QLabel(tr("步骤日志:")));
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
    connect(showDagBtn, &QPushButton::clicked, this, &MainWindow::onShowDAG);
    connect(showOriBtn, &QPushButton::clicked, this, &MainWindow::onShowOriginal);
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

    // Playback timer (used by Play/Pause).
    // Keep the interval moderate so the user can follow the algorithm visually.
    mPlayTimer.setInterval(260);
    connect(&mPlayTimer, &QTimer::timeout, this, &MainWindow::onPlayTick);

connect(view, &GraphView::edgeRequested, this, &MainWindow::onEdgeRequested);

    // 默认先创建一个
    onCreateGraph();
}

bool MainWindow::addEdgeImpl(int u, int v)
{
    // Editing the original graph while the DAG is displayed is confusing.
    // Force back to the original view so the user's mental model stays consistent.
    if (mShowingDag) onShowOriginal();

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

    // Graph topology changed -> SCC / DAG caches are no longer valid.
    mHasScc = false;
    mShowingDag = false;
    mSccRes = SCCResult();
    mDag = Graph();
    mPosOriginalSnapshot.clear();
    if (showDagBtn) showDagBtn->setEnabled(false);
    if (showOriBtn) showOriBtn->setEnabled(false);
    
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

    // New graph -> clear cached SCC/DAG results.
    mHasScc = false;
    mShowingDag = false;
    mSccRes = SCCResult();
    mDag = Graph();
    mPosOriginalSnapshot.clear();
    if (showDagBtn) showDagBtn->setEnabled(false);
    if (showOriBtn) showOriBtn->setEnabled(false);

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

    // SCC is defined on the original graph. If the condensed DAG is currently shown,
    // switch back first so the visualization matches the algorithm input.
    if (mShowingDag) onShowOriginal();

    // Ensure we start from a clean visualization state so SCC colors appear progressively.
    if (view) view->applyStep({StepType::ResetVisual, -1, -1, -1, 1, QString("Reset visual state")});
    // Run Tarjan SCC on the current directed graph and cache steps for playback.
    // The algorithm itself is pure; visualization happens in GraphView::applyStep().
    TarjanSCC tarjan;
    SCCResult res = tarjan.run(mGraph);

    // Cache SCC mapping for Step-5 (condense DAG) and later steps (Topo playback).
    mSccRes = res;
    mHasScc = true;
    mShowingDag = false;
    if (showDagBtn) showDagBtn->setEnabled(true);
    if (showOriBtn) showOriBtn->setEnabled(false);

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

void MainWindow::onShowDAG()
{
    if (!mHasScc) {
        statusBar()->showMessage(tr("Please run SCC first"), 2000);
        return;
    }

    // Once we switch to DAG view, Tarjan steps (node ids 1..n) no longer match the
    // displayed graph (node ids 1..sccCnt). Stop any ongoing playback to avoid
    // applying mismatched steps to the wrong view.
    mPlayTimer.stop();
    mPlaying = false;
    mSteps.clear();
    mStepIndex = 0;
    if (playBtn) { playBtn->setEnabled(false); playBtn->setText("播放"); }
    if (nextBtn) nextBtn->setEnabled(false);
    if (resetAlgoBtn) resetAlgoBtn->setEnabled(false);

    // 1) Snapshot current original node positions.
    // We intentionally use the *current* layout (after user dragging / force layout) so that
    // SCC centroids become an intuitive starting placement for the condensed DAG.
    mPosOriginalSnapshot = view->snapshotPositions(mGraph.n);

    // 2) Build the condensed graph (SCC graph / DAG).
    Condense cond;
    CondenseResult cRes = cond.run(mGraph, mSccRes.sccId, mSccRes.sccCnt);
    mDag = cRes.dag;

    // 3) Compute centroid position for each SCC as the initial DAG node position.
    const int C = mSccRes.sccCnt;
    QVector<QPointF> dagPos(C + 1);
    QVector<int> cnt(C + 1, 0);
    for (int i = 1; i <= mGraph.n; ++i) {
        const int cid = (i < (int)mSccRes.sccId.size()) ? mSccRes.sccId[i] : 0;
        if (cid <= 0 || cid > C) continue;
        dagPos[cid] += mPosOriginalSnapshot[i];
        cnt[cid] += 1;
    }
    for (int cid = 1; cid <= C; ++cid) {
        if (cnt[cid] > 0) dagPos[cid] = dagPos[cid] / cnt[cid];
    }

    // 4) Labels & colors.
    QStringList labels(C + 1);
    QVector<int> colorId(C + 1, 0);
    for (int cid = 1; cid <= C; ++cid) {
        labels[cid] = QString("S%1").arg(cid);
        colorId[cid] = cid; // each SCC gets a stable, deterministic palette color
    }

    // 5) Switch view.
    view->showGraphEx(mDag, dagPos, labels, colorId);
    mShowingDag = true;
    if (showDagBtn) showDagBtn->setEnabled(false);
    if (showOriBtn) showOriBtn->setEnabled(true);

    if (logEdit) {
        logEdit->append("----");
        logEdit->append(QString("Switched to DAG: %1 SCC nodes, %2 edges")
                        .arg(mDag.n)
                        .arg(mDag.edges.size()));
    }
    statusBar()->showMessage(tr("DAG view ready"), 2000);
}

void MainWindow::onShowOriginal()
{
    // Restore original graph view.
    QVector<QPointF> pos = mPosOriginalSnapshot;
    if (pos.isEmpty()) pos = mPos; // fallback to the initial circle placement

    // If we have SCC, keep SCC coloring on the original graph for continuity.
    QVector<int> colorId;
    if (mHasScc && (int)mSccRes.sccId.size() >= mGraph.n + 1) {
        colorId = QVector<int>(mGraph.n + 1, 0);
        for (int i = 1; i <= mGraph.n; ++i) colorId[i] = mSccRes.sccId[i];
    }

    view->showGraphEx(mGraph, pos, QStringList(), colorId);
    mShowingDag = false;
    if (showDagBtn) showDagBtn->setEnabled(mHasScc);
    if (showOriBtn) showOriBtn->setEnabled(false);
    statusBar()->showMessage(tr("Back to original graph"), 1500);
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
