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
