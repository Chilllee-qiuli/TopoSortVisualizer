/* ANNOTATED_FOR_STUDY
@file TopoKahn.h
@brief Kahn 拓扑排序（基于入度 + 队列）算法模块 + 可视化步骤输出。

注意：拓扑排序的输入必须是 DAG。
本项目里“保证 DAG”由前一步缩点得到（SCC 缩点后的图一定无环）。
*/

// 算法模块：拓扑排序（Kahn）
#pragma once
#include "Graph.h"
#include "Steps.h"
#include <vector>

struct TopoResult{
    bool ok = false;
    std::vector<int> order;
    std::vector<Step> steps;
};

// 枚举所有拓扑序（可能数量爆炸，适合课程小规模图）。
struct TopoAllResult{
    bool ok = false;
    std::vector<std::vector<int>> orders;
};

class TopoKahn{
public:
    TopoResult run(const Graph& dag);

    // 生成所有拓扑序列（回溯枚举）。
    // maxOrders < 0 表示不设上限；仅用于防止极端情况下卡死。
    TopoAllResult enumerateAll(const Graph& dag, int maxOrders = -1);

    // 给定一个拓扑序列 order，用它“驱动”Kahn 过程生成可视化 steps。
    // 这用于“每次播放只演示一个拓扑序”。
    TopoResult runWithOrder(const Graph& dag, const std::vector<int>& order);
};
