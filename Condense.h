/* ANNOTATED_FOR_STUDY
@file Condense.h
@brief 缩点（Condensation）：把 SCC 缩成 DAG 节点，得到无环图。

输入：
- 原图 g（n 个点）
- sccId：每个点属于哪个 SCC
- sccCnt：SCC 总数（缩点后 DAG 的节点数）

输出：
- dag：节点编号 1..sccCnt
- steps：这里记录“生成了一条缩点边”的事件，便于后续做缩点过程动画。
*/

// 算法模块：缩点（接口）
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
