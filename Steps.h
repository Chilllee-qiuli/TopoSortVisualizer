// algo/Steps.h
#pragma once
#include <QString>

enum class StepType {
    ResetVisual, // Clear visualization state (val=0 transient, val=1 also clear SCC)
    Visit, PushStack, PopStack,
    AssignSCC,
    BuildCondensedEdge,
    TopoInitIndeg,
    TopoEnqueue, TopoDequeue,
    TopoIndegDec
};

struct Step {
    StepType type;
    int u = -1;
    int v = -1;
    int scc = -1;     // AssignSCC 时用
    int val = 0;      // 例如入度变化后的值
    QString note;     // 右侧日志
};
