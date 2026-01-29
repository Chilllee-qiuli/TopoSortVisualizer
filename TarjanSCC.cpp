// 算法模块：强连通分量（Tarjan）
#include "TarjanSCC.h"
#include <algorithm>

SCCResult TarjanSCC::run(const Graph& g){
    G = &g; n = g.n;
    timer = sccCnt = 0;
    dfn.assign(n+1, 0);
    low.assign(n+1, 0);
    inStack.assign(n+1, 0);
    sccId.assign(n+1, 0);
    st.clear();
    steps.clear();

    // sccSize 下标从 1 开始，先占位
    sccSize.assign(n+1, 0);

    for(int i=1;i<=n;i++){
        if(!dfn[i]) dfs(i);
    }

    SCCResult res;
    res.sccCnt = sccCnt;
    res.sccId = sccId;
    res.sccSize.assign(sccCnt+1, 0);
    for(int i=1;i<=sccCnt;i++) res.sccSize[i] = sccSize[i];
    res.steps = steps;
    return res;
}

void TarjanSCC::dfs(int u){
    dfn[u] = low[u] = ++timer;
    steps.push_back({StepType::Visit, u, -1, -1, 0, QString("访问 %1").arg(u)});

    st.push_back(u);
    inStack[u] = 1;
    steps.push_back({StepType::PushStack, u, -1, -1, 0, QString("入栈 %1").arg(u)});

    for(int v: G->adj[u]){
        if(!dfn[v]){
            dfs(v);
            low[u] = std::min(low[u], low[v]);
        } else if(inStack[v]){
            low[u] = std::min(low[u], dfn[v]);
        }
    }

    if(low[u] == dfn[u]){
        ++sccCnt;
        while(true){
            int x = st.back(); st.pop_back();
            inStack[x] = 0;
            steps.push_back({StepType::PopStack, x, -1, -1, 0, QString("弹出 %1").arg(x)});

            sccId[x] = sccCnt;
            sccSize[sccCnt]++;
            steps.push_back({StepType::AssignSCC, x, -1, sccCnt, 0,
                             QString("添加节点 %1 到 SCC %2").arg(x).arg(sccCnt)});

            if(x == u) break;
        }
    }
}
