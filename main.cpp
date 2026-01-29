#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    // 需要的话可以设置窗口显示名：a.setApplicationDisplayName("TopoSortVisualizer-拓扑排序可视化");
    // QCoreApplication::setApplicationName("TopoSortVisualizer-拓扑排序可视化");
    MainWindow w;
    w.show();
    return a.exec();
}
