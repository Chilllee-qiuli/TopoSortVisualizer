// 数据结构：有向图
#pragma once
#include <vector>
#include <utility>

struct Graph {
    int n = 0;
    std::vector<std::vector<int>> adj;
    std::vector<std::pair<int,int>> edges; // 方便重建/缩点

    Graph() = default;
    Graph(int n): n(n), adj(n+1) {}

    void addEdge(int u, int v){
        adj[u].push_back(v);
        edges.push_back({u,v});
    }
};
