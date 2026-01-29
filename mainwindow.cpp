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
    // Dock 面板点 [x] 关闭时默认只是隐藏（除非设置了 WA_DeleteOnClose 才会销毁）。
    // 通过菜单提供开关，保证用户随时能把面板重新打开。
    QMenu* panelsMenu = menuBar()->addMenu(tr("Panels"));
    mGraphDockAction = mGraphDock->toggleViewAction();
    mAlgoDockAction  = mAlgoDock->toggleViewAction();

    // 一些可选的易用性快捷设置。
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

    // 连接“建图面板”的信号。
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
    // 只有在拿到有效 SCC 结果后，这些按钮才可用。
    showDagBtn = new QPushButton(tr("展示 DAG (缩点结果)"), algoPanel);
    showDagBtn->setEnabled(false);
    aLay->addWidget(showDagBtn);

    showOriBtn = new QPushButton(tr("回溯"), algoPanel);
    showOriBtn->setEnabled(false);
    aLay->addWidget(showOriBtn);

    aLay->addSpacing(8);
    aLay->addWidget(new QLabel(tr("Topo 排序可视化 (Kahn)")));

    runTopoBtn = new QPushButton(tr("开始拓扑排序 (Kahn)"), algoPanel);
    runTopoBtn->setEnabled(false);
    aLay->addWidget(runTopoBtn);

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

    // 放在建图面板下方，减少视觉拥挤。
    addDockWidget(Qt::RightDockWidgetArea, mAlgoDock);
    splitDockWidget(mGraphDock, mAlgoDock, Qt::Vertical);

    // 连接“算法面板”的信号。
    connect(runSccBtn, &QPushButton::clicked, this, &MainWindow::onRunSCC);
    connect(runTopoBtn, &QPushButton::clicked, this, &MainWindow::onRunTopo);
    connect(showDagBtn, &QPushButton::clicked, this, &MainWindow::onShowDAG);
    connect(showOriBtn, &QPushButton::clicked, this, &MainWindow::onShowOriginal);
    connect(playBtn, &QPushButton::clicked, this, &MainWindow::onPlayPause);
    connect(nextBtn, &QPushButton::clicked, this, &MainWindow::onNextStep);
    connect(resetAlgoBtn, &QPushButton::clicked, this, &MainWindow::onResetAlgo);

    // 菜单开关：Dock 被关闭后可通过菜单重新打开。
    setupPanelsMenu();
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    view = new GraphView(this);
    setCentralWidget(view);
// 面板（建图 + 算法）使用 QDockWidget 实现。
// 用户可能关闭面板；我们提供菜单开关，随时可重新打开。
setupDocks();

    // 播放计时器（用于 播放/暂停）。
    // 间隔不要太小，保证用户能看清算法过程。
    mPlayTimer.setInterval(260);
    connect(&mPlayTimer, &QTimer::timeout, this, &MainWindow::onPlayTick);

connect(view, &GraphView::edgeRequested, this, &MainWindow::onEdgeRequested);

    // 默认先创建一个
    onCreateGraph();
}

bool MainWindow::addEdgeImpl(int u, int v)
{
    // 当显示的是 DAG 时继续修改原图会让用户认知混乱。
    // 因此强制切回原图视图，保持“所见即所算”。
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

    // 图结构发生变化：已有 SCC / DAG 缓存不再有效。
    mHasScc = false;
    mShowingDag = false;
    mSccRes = SCCResult();
    mDag = Graph();
    mPosOriginalSnapshot.clear();
    if (showDagBtn) showDagBtn->setEnabled(false);
    if (showOriBtn) showOriBtn->setEnabled(false);
    if (runTopoBtn) runTopoBtn->setEnabled(false);
    if (runTopoBtn) runTopoBtn->setEnabled(false);
    
    // 图被修改后：应当清空缓存的 Steps，并重置 SCC 状态，避免显示过期结果。
    // 这样可以避免在新图上误用旧的 SCC 着色/播放步骤。
    if (view) view->applyStep({StepType::ResetVisual, -1, -1, -1, 1, QString()});
    onResetAlgo();
    return true;
}

void MainWindow::onCreateGraph()
{
    int n = nSpin->value();
    mGraph = Graph(n);
    mPos = makeCirclePos(n);

    // 新建图：清空 SCC/DAG 缓存结果。
    mHasScc = false;
    mShowingDag = false;
    mSccRes = SCCResult();
    mDag = Graph();
    mPosOriginalSnapshot.clear();
    if (showDagBtn) showDagBtn->setEnabled(false);
    if (showOriBtn) showOriBtn->setEnabled(false);
    if (runTopoBtn) runTopoBtn->setEnabled(false);

    uSpin->setRange(1, n);
    vSpin->setRange(1, n);

    view->showGraph(mGraph, mPos);
    updateEdgeCountUI();

    // 图变化时重置算法播放状态。
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

    // SCC 定义在原图上；如果当前显示的是缩点 DAG，需要先切回原图以匹配算法输入。
    // 先切回原图，保证“可视化对象”与“算法输入”一致。
    if (mShowingDag) onShowOriginal();

    // 从干净的可视化状态开始播放，这样 SCC 着色能按步骤逐步出现。
    if (view) view->applyStep({StepType::ResetVisual, -1, -1, -1, 1, QString("重置可视化状态")});
    // 对当前有向图运行 Tarjan SCC，并缓存步骤用于回放。
    // 算法本身保持纯净；可视化在 GraphView::applyStep() 中完成。
    TarjanSCC tarjan;
    SCCResult res = tarjan.run(mGraph);

    mAlgoMode = AlgoMode::TarjanSCC;
    mTopoRes = TopoResult();

    // 缓存 SCC 映射：供第 5 步缩点建 DAG，以及第 6 步拓扑回放使用。
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

    // 启用播放控制按钮。
    playBtn->setEnabled(!mSteps.isEmpty());
    nextBtn->setEnabled(!mSteps.isEmpty());
    resetAlgoBtn->setEnabled(true);

    statusBar()->showMessage(QString("Tarjan SCC 产生了 %1 个步骤").arg(mSteps.size()), 2500);

}

void MainWindow::onRunTopo()
{
    if (!mHasScc) {
        statusBar()->showMessage(tr("请先运行 SCC (Tarjan)"), 2000);
        return;
    }

    // 拓扑排序在缩点后的 DAG 上进行。
    // 若当前不是 DAG 视图，则先切换到 DAG（阶段切换需要确定性）。
    if (!mShowingDag) onShowDAG();
    if (!mShowingDag) return; // onShowDAG 可能因为 SCC 未就绪而失败。

    // 清理瞬态高亮（保留 DAG 节点的 SCC 调色板颜色）。
    if (view) view->applyStep({StepType::ResetVisual, -1, -1, -1, 0, QString("为拓扑排序重置")});

    TopoKahn topo;
    mTopoRes = topo.run(mDag);

    mSteps.clear();
    mSteps.reserve((int)mTopoRes.steps.size());
    for (const Step& s : mTopoRes.steps) mSteps.push_back(s);
    mStepIndex = 0;
    mAlgoMode = AlgoMode::TopoKahn;

    if (logEdit) {
        logEdit->clear();
        logEdit->append(QString("Topo sort on DAG: n=%1, m=%2").arg(mDag.n).arg(mDag.edges.size()));
        logEdit->append(QString("Result ok = %1").arg(mTopoRes.ok ? "true" : "false"));
        logEdit->append("----");
    }

    playBtn->setEnabled(!mSteps.isEmpty());
    nextBtn->setEnabled(!mSteps.isEmpty());
    resetAlgoBtn->setEnabled(true);
    playBtn->setText("播放");
    mPlaying = false;
    mPlayTimer.stop();

    statusBar()->showMessage(QString("Topo(Kahn) 产生了 %1 个步骤").arg(mSteps.size()), 2500);
}

void MainWindow::onShowDAG()
{
    if (!mHasScc) {
        statusBar()->showMessage(tr("请先运行 SCC (Tarjan)"), 2000);
        return;
    }

    // 一旦切到 DAG，Tarjan 的 step（节点编号 1..n）就不再匹配当前显示的图（节点 1..sccCnt）。
    // 因此必须停止正在播放的 step，避免把“错的步骤”应用到“错的视图”。
    // （否则会出现错误高亮甚至越界）。
    mPlayTimer.stop();
    mPlaying = false;
    mSteps.clear();
    mStepIndex = 0;
    mAlgoMode = AlgoMode::None;
    mTopoRes = TopoResult();
    mAlgoMode = AlgoMode::None;
    mTopoRes = TopoResult();
    if (playBtn) { playBtn->setEnabled(false); playBtn->setText("播放"); }
    if (nextBtn) nextBtn->setEnabled(false);
    if (resetAlgoBtn) resetAlgoBtn->setEnabled(false);

    // 1) 抓取当前原图节点坐标快照。
    // 这里刻意使用“当前布局”（包含用户拖拽/力导布局结果），
    // 这样每个 SCC 的质心会成为缩点 DAG 的自然初始位置。
    mPosOriginalSnapshot = view->snapshotPositions(mGraph.n);

    // 2) 构建缩点图（SCC 图 / DAG）。
    Condense cond;
    CondenseResult cRes = cond.run(mGraph, mSccRes.sccId, mSccRes.sccCnt);
    mDag = cRes.dag;

    // 3) 计算每个 SCC 的质心作为 DAG 节点初始位置。
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

    // 4) 准备标签与颜色组。
    QStringList labels(C + 1);
    QVector<int> colorId(C + 1, 0);
    for (int cid = 1; cid <= C; ++cid) {
        labels[cid] = QString("S%1").arg(cid);
        colorId[cid] = cid; // 每个 SCC 使用稳定的确定性调色板颜色
    }

    // 5) 切换视图显示 DAG。
    view->showGraphEx(mDag, dagPos, labels, colorId);
    mShowingDag = true;
    if (showDagBtn) showDagBtn->setEnabled(false);
    if (showOriBtn) showOriBtn->setEnabled(true);
    if (runTopoBtn) runTopoBtn->setEnabled(true);

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
    // 视图切换会使当前播放序列失效，应当停止播放并清理状态。
    mPlayTimer.stop();
    mPlaying = false;
    mSteps.clear();
    mStepIndex = 0;
    mAlgoMode = AlgoMode::None;
    mTopoRes = TopoResult();
    mAlgoMode = AlgoMode::None;
    if (playBtn) { playBtn->setEnabled(false); playBtn->setText("播放"); }
    if (nextBtn) nextBtn->setEnabled(false);
    if (resetAlgoBtn) resetAlgoBtn->setEnabled(false);
    // 恢复到原图视图。
    QVector<QPointF> pos = mPosOriginalSnapshot;
    if (pos.isEmpty()) pos = mPos; // fallback to the initial circle placement

    // 如果已有 SCC 结果，则在原图上保留 SCC 着色以保证视觉连贯性。
    QVector<int> colorId;
    if (mHasScc && (int)mSccRes.sccId.size() >= mGraph.n + 1) {
        colorId = QVector<int>(mGraph.n + 1, 0);
        for (int i = 1; i <= mGraph.n; ++i) colorId[i] = mSccRes.sccId[i];
    }

    view->showGraphEx(mGraph, pos, QStringList(), colorId);
    mShowingDag = false;
    if (showDagBtn) showDagBtn->setEnabled(mHasScc);
    if (showOriBtn) showOriBtn->setEnabled(false);
    if (runTopoBtn) runTopoBtn->setEnabled(false);
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

    // 结束条件。
    if (mStepIndex >= mSteps.size()) {
        mPlaying = false;
        mPlayTimer.stop();
        playBtn->setText("播放");

        if (mAlgoMode == AlgoMode::TopoKahn) {
            // 汇总输出最终拓扑序。
            if (logEdit) {
                logEdit->append("----");
                if (mTopoRes.ok) {
                    QStringList seq;
                    for (int x : mTopoRes.order) seq << QString::number(x);
                    logEdit->append(QString("拓扑序：%1").arg(seq.join(" ")));
                } else {
                    logEdit->append("拓扑失败：图中存在环（输出序列长度 < n）。");
                }
            }
            statusBar()->showMessage("拓扑步骤播放完成", 2000);
        } else {
            statusBar()->showMessage("SCC 步骤播放完成", 2000);
        }
    }
}

void MainWindow::onPlayTick()
{
    if (!mPlaying) return;
    onNextStep();
}

void MainWindow::onResetAlgo()
{
    // 停止播放并清理瞬态高亮，但保留当前图数据。
    mPlayTimer.stop();
    mPlaying = false;
    if (playBtn) playBtn->setText("播放");

    mSteps.clear();
    mStepIndex = 0;
    mAlgoMode = AlgoMode::None;
    mTopoRes = TopoResult();

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
