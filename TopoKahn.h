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

class TopoKahn{
public:
    TopoResult run(const Graph& dag);
};
