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

namespace {
// 回溯枚举所有拓扑序。
static void dfsAllTopo(
        const Graph& g,
        std::vector<int>& indeg,
        std::vector<char>& used,
        std::vector<int>& cur,
        std::vector<std::vector<int>>& out,
        bool& ok,
        int maxOrders)
{
    const int n = g.n;
    if ((int)cur.size() == n) {
        out.push_back(cur);
        return;
    }

    // 找所有当前可选（未使用且入度为 0）的点。
    std::vector<int> cand;
    cand.reserve(n);
    for (int i = 1; i <= n; ++i) {
        if (!used[i] && indeg[i] == 0) cand.push_back(i);
    }

    if (cand.empty()) {
        // 还有点未放入序列，但没有可选点，说明存在环。
        ok = false;
        return;
    }

    for (int u : cand) {
        if (maxOrders >= 0 && (int)out.size() >= maxOrders) return;

        used[u] = 1;
        cur.push_back(u);

        // 移除 u 对后继入度的影响
        for (int v : g.adj[u]) indeg[v]--;

        dfsAllTopo(g, indeg, used, cur, out, ok, maxOrders);
        if (!ok) {
            // 仍需回溯恢复 indeg
        }

        // 回溯恢复
        for (int v : g.adj[u]) indeg[v]++;
        cur.pop_back();
        used[u] = 0;
    }
}
} // namespace

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

TopoAllResult TopoKahn::enumerateAll(const Graph& dag, int maxOrders)
{
    const int n = dag.n;
    std::vector<int> indeg(n + 1, 0);
    for (int u = 1; u <= n; ++u) {
        for (int v : dag.adj[u]) indeg[v]++;
    }

    TopoAllResult res;
    res.ok = true;
    std::vector<char> used(n + 1, 0);
    std::vector<int> cur;
    cur.reserve(n);
    dfsAllTopo(dag, indeg, used, cur, res.orders, res.ok, maxOrders);
    return res;
}

TopoResult TopoKahn::runWithOrder(const Graph& dag, const std::vector<int>& order)
{
    const int n = dag.n;
    std::vector<int> indeg(n + 1, 0);
    for (int u = 1; u <= n; ++u) {
        for (int v : dag.adj[u]) indeg[v]++;
    }

    std::vector<char> removed(n + 1, 0);
    std::vector<Step> steps;
    steps.reserve(4 * (n + (int)dag.edges.size()));

    // 1) 初始化入度
    for (int i = 1; i <= n; ++i) {
        steps.push_back({StepType::TopoInitIndeg, i, -1, -1, indeg[i],
                         QString("初始化入度 indeg[%1]=%2").arg(i).arg(indeg[i])});
    }

    // 2) 初始可选（入度为 0）节点
    for (int i = 1; i <= n; ++i) {
        if (indeg[i] == 0) {
            steps.push_back({StepType::TopoEnqueue, i, -1, -1, 0,
                             QString("入度为 0，加入候选 %1").arg(i)});
        }
    }

    std::vector<int> out;
    out.reserve(n);

    // 3) 按给定 order 依次选择
    bool ok = ((int)order.size() == n);
    for (int k = 0; k < (int)order.size() && k < n; ++k) {
        const int u = order[k];
        if (u < 1 || u > n || removed[u]) { ok = false; break; }
        if (indeg[u] != 0) { ok = false; break; }

        removed[u] = 1;
        out.push_back(u);
        steps.push_back({StepType::TopoDequeue, u, -1, -1, 0,
                         QString("选择 %1 作为本序列第 %2 个输出").arg(u).arg(k + 1)});

        for (int v : dag.adj[u]) {
            if (removed[v]) continue;
            indeg[v]--;
            steps.push_back({StepType::TopoIndegDec, u, v, -1, indeg[v],
                             QString("处理边 %1->%2, indeg[%2]-- => %3").arg(u).arg(v).arg(indeg[v])});
            if (indeg[v] == 0) {
                steps.push_back({StepType::TopoEnqueue, v, -1, -1, 0,
                                 QString("入度变为 0，加入候选 %1").arg(v)});
            }
        }
    }

    TopoResult res;
    res.ok = ok && ((int)out.size() == n);
    res.order = out;
    res.steps = steps;
    return res;
}
