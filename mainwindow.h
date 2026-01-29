#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "GraphView.h"
#include "Graph.h"
#include <QDockWidget>
#include <QSpinBox>
#include <QPushButton>
#include <QTextEdit>
#include <QTimer>
#include <QLabel>
#include <QAction>
#include "Steps.h"
#include "TarjanSCC.h"
#include "Condense.h"
#include "TopoKahn.h"

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


// --- Dock 面板 ---
QDockWidget* mGraphDock = nullptr;
QDockWidget* mAlgoDock  = nullptr;
QAction* mGraphDockAction = nullptr;
QAction* mAlgoDockAction  = nullptr;

// 边统计相关的 界面
QLabel* edgeCountLabel = nullptr;

void setupDocks();
void setupPanelsMenu();
void updateEdgeCountUI();

    void onCreateGraph();
    void onAddEdge();
    void onAddEdgesFromText();
    void onEdgeRequested(int u, int v);

    // 算法控制（第 4 步：Tarjan SCC 可视化）
    void onRunSCC();
    void onShowDAG();
    void onShowOriginal();

    // 第 6 步：在缩点 DAG 上进行拓扑排序回放（Kahn）。
    void onRunTopo();

    void onPlayPause();
    void onNextStep();
    void onResetAlgo();
    void onPlayTick();

private:
    Graph mGraph;
    QVector<QPointF> mPos;

    QSpinBox* nSpin = nullptr;
    QSpinBox* uSpin = nullptr;
    QSpinBox* vSpin = nullptr;
    QTextEdit* edgesEdit = nullptr;

    // --- 算法回放状态 ---
    QVector<Step> mSteps;
    int mStepIndex = 0;
    bool mPlaying = false;
    QTimer mPlayTimer;

    enum class AlgoMode { None, TarjanSCC, TopoKahn };
    AlgoMode mAlgoMode = AlgoMode::None;

    // 最近一次拓扑排序的结果缓存（用于最终输出序列）。
    TopoResult mTopoRes;

    // --- 算法相关 界面 控件 ---
    QPushButton* runSccBtn = nullptr;
    QPushButton* runTopoBtn = nullptr;
    QPushButton* playBtn = nullptr;
    QPushButton* nextBtn = nullptr;
    QPushButton* resetAlgoBtn = nullptr;
    QTextEdit* logEdit = nullptr;

    // 第 5 步 界面（切换到缩点 DAG 视图）
    QPushButton* showDagBtn = nullptr;
    QPushButton* showOriBtn = nullptr;

    // --- 缓存的算法结果（跨阶段复用） ---
    bool mHasScc = false;
    bool mShowingDag = false;
    SCCResult mSccRes;         // SCC mapping for the current original graph
    Graph mDag;                // condensed DAG
    QVector<QPointF> mPosOriginalSnapshot; // positions used to compute SCC centroids

    bool addEdgeImpl(int u, int v); // 统一入口

};
#endif // MAINWINDOW_H
