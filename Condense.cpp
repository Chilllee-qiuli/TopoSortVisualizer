/* ANNOTATED_FOR_STUDY
@file Condense.cpp
@brief 缩点实现：遍历原图边表，把跨 SCC 的边映射到 SCC 图，并去重。

为什么要“去重”？
- 原图中可能有多条边最终映射到同一条 SCC 边（例如 A->B 的多条边）。
- DAG 中我们只画一条即可，否则入度/动画会被重复统计。
*/

// 算法模块：缩点
#include "Condense.h"
#include <unordered_set>

static long long key(int a,int b){ return ( (long long)a<<32 ) ^ (unsigned)b; }

CondenseResult Condense::run(const Graph& g, const std::vector<int>& sccId, int sccCnt){
    Graph dag(sccCnt);
    std::vector<Step> steps;

    std::unordered_set<long long> seen; // 去重 dag 边
    for(auto [u,v]: g.edges){
        int su = sccId[u], sv = sccId[v];
        if(su == sv) continue;
        long long k = key(su, sv);
        if(seen.insert(k).second){
            dag.addEdge(su, sv);
            steps.push_back({StepType::BuildCondensedEdge, su, sv, -1, 0,
                             QString("缩点边：SCC%1 -> SCC%2").arg(su).arg(sv)});
        }
    }

    return {dag, steps};
}
