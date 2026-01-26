// algo/Condense.h
#pragma once
#include "Graph.h"
#include "Steps.h"
#include <vector>


struct CondenseResult{
    Graph dag;
    std::vector<Step> steps;
};

class Condense {
public:
    CondenseResult run(const Graph& g, const std::vector<int>& sccId, int sccCnt);
};
