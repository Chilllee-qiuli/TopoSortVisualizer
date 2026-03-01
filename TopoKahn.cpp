/* ANNOTATED_FOR_STUDY
@file TopoKahn.cpp
@brief Kahn 拓扑排序实现 + Step 记录。

可视化对应关系：
- TopoInitIndeg(i,val)：初始化并显示每个点的入度
- TopoEnqueue(i)：入度为 0 的点入队（界面用蓝色标记 queued）
- TopoDequeue(i)：出队并输出（界面用绿色 done，并显示输出序号 1..k）
- TopoIndegDec(u,v,val)：处理边 u->v 时，v 的入度减 1（界面高亮这条边并刷新 v 下方的入度文字）
*/

// 算法模块：拓扑排序（Kahn）
#include "TopoKahn.h"
#include <queue>

TopoResult TopoKahn::run(const Graph& dag){
    int n = dag.n;
    std::vector<int> indeg(n+1, 0);
    for(int u=1;u<=n;u++){
        for(int v: dag.adj[u]) indeg[v]++;
    }

    std::vector<Step> steps;
    for(int i=1;i<=n;i++){
        steps.push_back({StepType::TopoInitIndeg, i, -1, -1, indeg[i],
                         QString("初始化入度 indeg[%1]=%2").arg(i).arg(indeg[i])});
    }

    std::queue<int> q;
    for(int i=1;i<=n;i++){
        if(indeg[i]==0){
            q.push(i);
            steps.push_back({StepType::TopoEnqueue, i, -1, -1, 0,
                             QString("入队 %1").arg(i)});
        }
    }

    std::vector<int> order;
    while(!q.empty()){
        int u=q.front(); q.pop();
        order.push_back(u);
        steps.push_back({StepType::TopoDequeue, u, -1, -1, 0,
                         QString("出队 %1").arg(u)});

        for(int v: dag.adj[u]){
            indeg[v]--;
            steps.push_back({StepType::TopoIndegDec, u, v, -1, indeg[v],
                             QString("indeg[%1]-- => %2").arg(v).arg(indeg[v])});
            if(indeg[v]==0){
                q.push(v);
                steps.push_back({StepType::TopoEnqueue, v, -1, -1, 0,
                                 QString("入队 %1").arg(v)});
            }
        }
    }

    TopoResult res;
    res.ok = ((int)order.size()==n);
    res.order = order;
    res.steps = steps;
    return res;
}
