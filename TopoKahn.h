// algo/TopoKahn.h
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
