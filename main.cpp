#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    // a.setApplicationDisplayName("TopoSortVisualizer-拓扑排序可视化");
    // QCoreApplication::setApplicationName("TopoSortVisualizer-拓扑排序可视化");
    MainWindow w;
    w.show();
    return a.exec();
}
