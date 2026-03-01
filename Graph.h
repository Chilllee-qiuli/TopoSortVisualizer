/* ANNOTATED_FOR_STUDY
纯数据结构

原则：Graph / Tarjan / 缩点 / 拓扑排序全部不包含 Qt 类型，Qt 只在 GraphView/MainWindow 层出现。
故 Graph 不写 Qt 容器

约定：
- 节点编号是 1..n（下标 0 不用）
- adj[u] 存 u 的所有出边终点。
- edges 保存边表（插入顺序），缩点/重建画面时会更方便。
*/

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
