/* ANNOTATED_FOR_STUDY
@file TarjanSCC.h
@brief Tarjan 强连通分量（SCC）算法模块。输出 SCC 映射 + 可视化步骤。

这里写成一个算法库：
- 输入：Graph（1..n）
- 输出：每个点属于哪个 SCC（sccId），共有多少 SCC（sccCnt）
- 同时输出 steps：把 DFS 的关键动作（Visit/Push/Pop/AssignSCC）记录下来，供界面回放。
*/

// 算法模块：强连通分量（Tarjan）
#pragma once
#include "Graph.h"
#include "Steps.h"
#include <vector>

struct SCCResult {
    int sccCnt = 0;
    std::vector<int> sccId;   // 1..n -> 1..sccCnt
    std::vector<int> sccSize; // 1..sccCnt
    std::vector<Step> steps;
};

class TarjanSCC {
public:
    SCCResult run(const Graph& g);

private:
    const Graph* G = nullptr;
    int n = 0, timer = 0, sccCnt = 0;

    std::vector<int> dfn, low, st;
    std::vector<char> inStack;
    std::vector<int> sccId, sccSize;

    std::vector<Step> steps;

    void dfs(int u);
};
