#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "GraphView.h"
#include "Graph.h"
#include <QDockWidget>
#include <QSpinBox>
#include <QPushButton>
#include <QTextEdit>
#include <QLabel>
#include <QTimer>
#include <QComboBox>
#include <QStackedWidget>

#include "Steps.h"
#include "TarjanSCC.h"
#include "TopoKahn.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT
    QDockWidget* controlsDock = nullptr;

    // Dock 内部：顶部切换 + 堆叠页
    QComboBox*     panelModeCombo = nullptr;   // 0=建图, 1=算法
    QStackedWidget* panelStack = nullptr;

    // 建图页 / 算法页公共信息
    QLabel* edgeCountLabel = nullptr;          // 建图页显示边数
    QLabel* graphInfoAlgoLabel = nullptr;      // 算法页显示 n/边数

    void updateEdgeCount();

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

    Graph mGraph;
    QVector<QPointF> mPos;

    // ===== 建图页控件 =====
    QSpinBox* nSpin = nullptr;
    QSpinBox* uSpin = nullptr;
    QSpinBox* vSpin = nullptr;
    QTextEdit* edgesEdit = nullptr;

    bool addEdgeImpl(int u, int v); // 统一入口

    // ===== 算法页控件 =====
    QVector<Step> mSteps;
    int mStepIdx = 0;
    QTimer mPlayTimer;

    QPushButton* btnRunScc = nullptr;
    QPushButton* btnRunTopo = nullptr;
    QPushButton* btnNext = nullptr;
    QPushButton* btnPlay = nullptr;
    QSpinBox* speedSpin = nullptr;
    QTextEdit* logEdit = nullptr;

    void loadSteps(const std::vector<Step>& steps);

private slots:
    void onRunSccDemo();
    void onRunTopoDemo();
    void onNextStep();
    void onTogglePlay();
};

#endif // MAINWINDOW_H
