/* ANNOTATED_FOR_STUDY
@file Steps.h
@brief “算法 -> 可视化”的桥梁：把算法过程拆成一系列离散 Step。

核心思想：
- 算法层只负责生成 Step 序列（纯逻辑）。
- 可视化层(GraphView)只负责“解释 Step 并改变界面状态”。
- 这样以后想换算法（Kosaraju / DFS topo / etc）只要改 Step 生成；
  想换可视化风格（颜色/动画）只要改 GraphView::applyStep()。

约定：
- type : 事件类型（访问节点/入栈/出栈/分配 SCC/入度变化/入队/出队...）
- u,v  : 事件关联的节点/边端点（没有就为 -1）
- scc  : AssignSCC 时使用（把 u 归到哪个 SCC）
- val  : 通用数值（这里主要用于记录当前入度 indeg[v]）
- note : 右侧日志显示的文字（MainWindow 里 append 到 QTextEdit）
*/

// 可视化步骤定义
#pragma once
#include <QString>

/*
采用enum的好处：
作用域隔离：枚举成员（如 Visit, PushStack）位于 StepType 的作用域内。使用时必须写 StepType::Visit，不能直接写 Visit。这避免了命名冲突（例如，如果有另一个枚举也有 Reset，不会冲突）。
类型安全：它不会隐式转换为整数 (int)。如果需要获取整数值，必须显式转换：static_cast<int>(StepType::Visit)。
底层类型：默认底层类型通常是 int，但可以指定（如 enum class StepType : uint8_t）。
 */
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
