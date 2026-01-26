// algo/TarjanSCC.h
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
