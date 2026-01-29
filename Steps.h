// 可视化步骤定义
#pragma once
#include <QString>

enum class StepType {
    ResetVisual, // 清理可视化状态（val=0 仅清理“瞬态高亮”，val=1 额外清空 SCC 着色）
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
