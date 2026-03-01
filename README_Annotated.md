# TopoSortVisualizer (Annotated Notes)

> 这份 README 是给“只写过传统 C++ 算法题、没写过 Qt 工程”的你准备的：
> 先告诉你整个项目的**框架怎么串起来**，再告诉你**从哪里开始读代码**。

## 1. 你要实现的课程题目（项目做了什么）

输入任意有向图 G：
1) **Tarjan SCC**：把原图分成若干强连通分量（每个 SCC 用一种颜色表示）  
2) **缩点**：每个 SCC 缩成一个点，得到 **DAG**  
3) **拓扑排序**：对 DAG 做 Kahn 拓扑排序，并逐步高亮“入队/出队/入度变化”

项目里的可视化不是一次性画最终答案，而是把过程拆成一个个 Step 来播放。

## 2. 三层架构（读懂就不怕工程了）

**(A) 数据层**：`Graph.h`  
- 纯 C++ 数据结构：`adj` + `edges`，节点编号 1..n

**(B) 算法层**：`TarjanSCC.*` / `Condense.*` / `TopoKahn.*`  
- 纯 C++ 算法实现  
- 额外输出：`std::vector<Step> steps`（算法过程事件序列）

**(C) 界面层**：`MainWindow.*` + `GraphView.*`  
- MainWindow：按钮/面板/播放控制（控制器）  
- GraphView：真正“画图”和“解释 Step 并改变画面”

一句话总结：  
> **算法只产出 Steps；GraphView 负责把 Steps 变成动画。**

## 3. 关键 Qt 概念（只需要懂这些就能继续写）

### 3.1 信号/槽（signals/slots）
- “用户点了按钮”不是你主动 while 读取，而是 Qt 发出一个信号。
- 你用 `connect(btn, &QPushButton::clicked, this, &MainWindow::onRunSCC);`
  把信号连到槽函数（回调）。

### 3.2 事件循环（QApplication::exec）
- `main.cpp` 里 `a.exec()` 开始事件循环。
- 之后鼠标/键盘/定时器都会通过事件循环回调你的代码。

### 3.3 QGraphicsView / QGraphicsScene / QGraphicsItem
- Scene：一个二维舞台  
- Item：舞台上的演员（节点圆形/边/文字）  
- View：相机（负责显示、缩放、坐标变换、鼠标事件）

本项目：
- NodeItem = QGraphicsEllipseItem（圆）+ QObject（为了 signals）  
- EdgeItem = QGraphicsPathItem（带箭头路径）+ QObject（方便 slot 更新路径）

### 3.4 QTimer（定时器）
- `mPlayTimer`：每隔 260ms 播放一步 step（MainWindow::onPlayTick）  
- `mForceTimer`：每隔 16ms 跑一次力导布局（GraphView::onForceTick）

## 4. 从哪里开始读代码（推荐顺序）

1) `main.cpp`：看 Qt 程序如何启动  
2) `mainwindow.h/.cpp`：看按钮如何触发算法、如何保存 steps、如何播放  
3) `Steps.h`：看 StepType 有哪些事件，Step 里有哪些字段  
4) `TarjanSCC.cpp`：看 Tarjan 如何记录 steps  
5) `GraphView::applyStep()`：看每个 StepType 如何改变界面状态  
6) `GraphView::onForceTick()`：看“灵动”的力导布局怎么做

## 5. 你以后怎么扩展（按你的课程需求加更多动画）

### 5.1 新增 StepType
在 `Steps.h` 里加枚举值，例如：
- `HighlightEdge`
- `MarkCycle`
- `TopoQueueFront` ...

### 5.2 算法里 push step
在算法代码里 `steps.push_back({StepType::..., ...});`

### 5.3 GraphView 里实现对应效果
在 `GraphView::applyStep()` 的 switch 里加 case，
修改 item 的 `setData(role, value)`，然后 `resetStyle()` 就能稳定重绘。

## 6. 如何编译运行（推荐 Qt Creator）

1) Qt Creator 打开 `CMakeLists.txt`  
2) 选择 Kit（MSVC/MinGW/macOS clang...）  
3) Build & Run  

如果你用命令行：
```bash
cmake -S . -B build
cmake --build build
```
然后运行生成的可执行文件。

---

如果你希望我继续帮你：  
- 把“缩点过程”也做成可播放 steps（逐条生成 DAG 边并高亮）  
- 给 GraphView 加一个“参数面板”（实时调 repulsion/springK 等）  
- 做类似 csacademy 的“自动排列 + 拖动更丝滑”
都可以在现有架构上渐进式加。
