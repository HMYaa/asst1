# 作业 1：四核 CPU 上的性能分析

**截止时间：10 月 6 日（周一）23:59**

**总分 100 分 + 6 分额外加分**

## 概述

本次作业旨在帮助你理解现代多核 CPU 中两类主要并行执行方式：

1. 单个处理器核心内部的 SIMD 执行
2. 多核心并行执行（你也会看到 Intel 超线程的影响）

你还将获得并行程序性能测量与分析的实践经验（这项能力有挑战，但非常重要，整门课都会用到）。本次作业代码量不大，但分析量很大。

## 环境配置

__本次作业需要在新的 myth 机器上运行代码__  
（主机名：`myth[51-66].stanford.edu`）。如果你在 myth 上没有 home 目录，请在[这里](https://stanford.service-now.com/it_services?id=sc_cat_item&sys_id=cab169801bd918d0685d4377cc4bcbe0)提交 HelpSU 工单。

<!-- 这些机器使用四核 4.2GHz 的 Intel Core i7 处理器（动态频率在条件允许时可升至 4.5GHz）。每个核心支持 __2 个硬件线程__（Intel 称为 Hyper-Threading），并支持 AVX2 向量指令，可在一个向量中同时执行 __8 路 SIMD__ 单精度计算。   -->
CPU 规格可见：<https://www.intel.com/content/www/us/en/products/sku/97129/intel-core-i77700k-processor-8m-cache-up-to-4-50-ghz/specifications.html>  
想深入了解可看：[Kaby Lake 架构介绍](https://en.wikichip.org/wiki/intel/microarchitectures/kaby_lake)。

注意：评分时以你在 Stanford myth 机器上跑出的性能为准。你也可以在自己机器上运行本作业程序（需要先安装 ISPC：<http://ispc.github.io/>）。如果在报告中加入其他机器上的结果，请明确标注机器信息。

开始步骤：

1. 本作业多个程序需要 ISPC。可按下列步骤在 myth 上安装：

   在 myth 上把 Linux 二进制下载到你选择的目录。可从 ISPC 下载页获取二进制：<https://ispc.github.io/downloads.html>。推荐在 myth 上直接使用 `wget` 下载。  
   截至 2025 秋季第 1 周，以下命令可用：

   ```bash
   wget https://github.com/ispc/ispc/releases/download/v1.28.1/ispc-v1.28.1-linux.tar.gz
   ```

   解压：

   ```bash
   tar -xvf ispc-v1.28.1-linux.tar.gz
   ```

   将 ISPC 的 `bin` 目录加入 `PATH`。例如：

   ```bash
   export PATH=$PATH:${HOME}/Downloads/ispc-v1.28.1-linux/bin
   ```

   可把上面这行加入 `.bashrc` 以永久生效。  
   若你使用 csh，请用 `setenv` 设置 `PATH`。

2. 作业起始代码仓库：

   ```bash
   git clone https://github.com/stanford-cs149/asst1.git
   ```

## Program 1：用线程并行生成分形（20 分）

进入 `prog1_mandelbrot_threads/` 编译并运行（`make`，然后 `./mandelbrot`）。  
程序会生成 `mandelbrot-serial.ppm`，它是著名 Mandelbrot 集的可视化图像。多数平台都能查看 `.ppm`。远程看图需要 X server：

- Linux：通常无需额外下载
- Mac：可用 [Xquartz](https://www.xquartz.org/)
- Windows：可用 [VcXsrv](https://sourceforge.net/projects/vcxsrv/)

启用 SSH X-Forwarding 后，用 `ssh -Y` 登录 myth，再用 `display` 命令看图。图中每个像素对应复平面的一个点，亮度与判断该点是否属于 Mandelbrot 集所需计算代价成正比。  
可加 `--view 2` 生成第二视角（参见 `mandelbrotSerial.cpp` 中 `mandelbrotSerial()`）。  
Mandelbrot 集定义可见：<http://en.wikipedia.org/wiki/Mandelbrot_set>。

![Mandelbrot Set](handout-images/mandelbrot_viz.jpg "两个 Mandelbrot 视角。像素计算代价与亮度成正比。Program 1 和 3 可用 --view 2。")

你的任务：使用 [`std::thread`](https://en.cppreference.com/w/cpp/thread/thread) 并行图像计算。  
起始代码在 `mandelbrotThread.cpp` 的 `mandelbrotThread()`，里面主线程通过 `std::thread(function, args...)` 启动一个额外线程，再调用 `join` 等待。当前这个线程不做实际计算，马上返回。你需要在 `workerThreadStart` 中补上工作逻辑。本作业不需要使用其他 `std::thread` API。

**你需要完成：**

1. 修改起始代码，先实现 2 个处理器并行：线程 0 算图像上半部分，线程 1 算下半部分。这叫 _空间划分（spatial decomposition）_。
2. 扩展到 2~8 线程，按块划分图像任务。处理器虽仅 4 核，但每核支持 2 个硬件线程，因此最多可交错执行 8 线程。  
   在报告中，针对 __view 1__ 画出“相对串行基线的加速比”随线程数变化图。加速是否线性？为什么（或为什么不）？  
   （可额外画 view 2 辅助分析，提示：留意 3 线程数据点）
3. 为验证你的假设，在 `workerThreadStart()` 开头和结尾加计时，测每个线程耗时。解释这些测量如何说明前面的加速比曲线。
4. 改进线程任务映射，使两种 view 下都达到 __约 7~8x__ 加速（超过 7x 即可）。  
   不允许在线程间同步；需要一个统一划分策略，能对所有线程数都效果良好，不能对每个配置硬编码。  
   （提示：存在很简单的静态分配方案，不需通信/同步）  
   在报告中说明你的并行策略，并汇报最终 8 线程加速比。
5. 用改进后的代码跑 16 线程。性能是否明显优于 8 线程？为什么？

## Program 2：使用 SIMD Intrinsics 向量化（20 分）

查看 `prog2_vecintrin/main.cpp` 中 `clampedExpSerial`。  
`clampedExp()` 对每个元素计算 `values[i] ^ exponents[i]`，并把结果截断到 9.999999。  
你的任务是把这段代码向量化，让它可在 SIMD 机器上运行。

但你无需直接写 SSE/AVX2 真实指令，而是使用课程提供的“伪向量指令”库 `CS149intrin.h`。该库提供向量寄存器和 mask 操作（不会直接映射真实 CPU 指令，而是模拟执行并给出便于调试的反馈）。  
`main.cpp` 里给了 `abs()` 的向量化示例，展示了 load/store 与 mask 的基本用法。注意：该 `abs()` 示例并不处理所有输入（你可以自己找原因）。建议阅读 `CS149intrin.h` 的注释和 API。

实现提示：

- 每条向量指令都可带可选 mask。mask 中某 lane 为 0 表示该 lane 被屏蔽，此次指令不会覆盖该 lane 的输出值。未显式给 mask 等价于全 1 mask。
- 你会需要多个 mask 寄存器及其组合操作。
- `_cs149_cntbits` 对本题很有帮助。
- 注意循环次数 `N` 不是 `VECTOR_WIDTH` 整数倍的情况。建议测试 `./myexp -s 3`。`_cs149_init_ones` 可能有用。
- 可用 `./myexp -l` 查看指令执行日志。可用 `addUserLog()` 增加自定义日志，也可用 `CS149Logger.printLog()` 辅助调试。

程序会检查正确性。若错误，会输出第一个错误位置及输入输出对照（`output =` 应与 `gold =` 一致）。程序还会输出向量单元统计信息。  
性能可用 `Total Vector Instructions` 评估（可认为每条伪向量指令耗时 1 cycle）。`Vector Utilization` 表示 lane 启用比例。

**你需要完成：**

1. 在 `clampedExpVector` 中实现 `clampedExpSerial` 的向量化版本，要求适配任意 `N` 与 `VECTOR_WIDTH`。
2. 运行 `./myexp -s 10000`，把 `VECTOR_WIDTH` 从 2、4、8、16 扫一遍（改 `CS149intrin.h` 中宏定义），记录向量利用率。分析利用率随向量宽度是升/降/不变，以及原因。
3. _加分（1 分）_：在 `arraySumVector` 中实现 `arraySumSerial` 的向量化版本。可假设 `N` 是 `VECTOR_WIDTH` 的倍数。目标复杂度从 `O(N)` 提升到约 `(N / VECTOR_WIDTH + VECTOR_WIDTH)`，甚至 `(N / VECTOR_WIDTH + log2(VECTOR_WIDTH))`。`hadd` 和 `interleave` 可能有用。

## Program 3：使用 ISPC 并行生成分形（20 分）

现在回到 Mandelbrot（类似 Program 1），但会进一步利用：

- 核内 SIMD
- 多核并行

Program 1 中你通过线程显式把工作映射到核心。Program 3 使用 ISPC 语言构造来描述 _彼此独立_ 的计算，让编译器和运行时负责高效利用并行硬件。  
在 Mandelbrot 场景下，每个像素计算彼此独立。

你要修复 Program 3 中一个简单问题（会影响性能但不影响正确性）。修复后，性能应超过串行 `mandelbrotSerial()` 的 32 倍。

### Program 3 - Part 1：ISPC 基础（20 分中的 10 分）

阅读 ISPC 代码时要记住：语法像 C/C++，但执行模型不同。  
与 C 不同，ISPC 程序的多个 program instance 总是并行执行在 SIMD 单元上。并行实例数由编译器根据目标机器决定，程序可通过内置变量 `programCount` 获取；当前实例 ID 用 `programIndex`。  
从 C 调 ISPC 函数，可以视为启动一组并行实例（ISPC 文档称 gang）。该 gang 运行结束后控制权返回 C 代码。

__请再读一遍上段，这非常重要。__

然后 README 给了 C + ISPC 的向量加法示例（`sum` 和 `sum2`），核心对比：

- `sum`：命令式，描述如何把工作映射到实例
- `sum2`：声明式，描述独立工作集合，具体分配交给 ISPC

建议先看 ISPC 官方 walkthrough：<http://ispc.github.io/example.html>。示例程序与本作业 `mandelbrot.ispc` 的 `mandelbrot_ispc()` 非常接近。

**你需要完成：**

1. 编译并运行 `mandelbrot_ispc`。当前 ISPC 配置发射 8 路 AVX2。  
   基于 CPU 特性，你预期理论最大加速是多少？为什么实测可能低于理想值？  
   （提示：考虑计算本身特性、图像中对 SIMD 不友好的区域；比较不同 view 的渲染表现）  
   注意：本小节中 ISPC 并行发生在单核 SIMD 内，与 Program 1 的多核线程并行不同。

补充：若查 CPU 微架构资料会发现每周期标量/向量指令吞吐规则很复杂。本作业可近似认为 8 路向量浮点执行资源数量与标量执行资源数量“差不多”。

### Program 3 - Part 2：ISPC Tasks（20 分中的 10 分）

除了 `foreach` 的 SIMD 并行，ISPC 还提供跨核心并行机制：_ISPC tasks_。

查看 `mandelbrot_ispc_withtasks` 中 `launch[2]`：它启动 2 个 task。每个 task 内会有一组 ISPC 实例并行执行。`mandelbrot_ispc_task` 里每个 task 负责图像一个区域。  
类似 `foreach` 的“迭代可任意顺序执行”，这些 task 也可任意顺序调度，并能在不同 CPU 核心并行。

**你需要完成：**

1. 运行 `mandelbrot_ispc --tasks`。记录 view 1 的加速比，以及相对不使用 tasks 的 `mandelbrot_ispc` 的提升。
2. 仅修改 `mandelbrot_ispc_withtasks()` 中 task 数量，即可明显提速，达到超过串行版 32x。  
   说明你如何确定 task 数，以及为何该数值效果最好。
3. _加分（2 分）_：比较 Program 1 线程抽象与 ISPC task 抽象的差异。  
   可用思考题：启动 10000 个 ISPC tasks 会怎样？启动 10000 个线程会怎样？（请讨论一般情况，不限于本 Mandelbrot 程序）

额外思考：  
为什么 ISPC 同时需要 `foreach` 和 `launch` 两种并行机制？  
答案：这是很好的问题，欢迎来 office hours 讨论。

## Program 4：迭代版 `sqrt`（15 分）

Program 4 是一个 ISPC 程序，计算 2000 万个 [0, 3] 区间随机数的平方根。实现使用牛顿法求解方程 `${\frac{1}{x^2}} - S = 0`，初值为 1.0。  
README 图示了在 (0,3) 区间内收敛所需迭代次数：初值越接近真实值，收敛越快。超出该范围实现不保证收敛。

注：本题用于复习 Program 2 和 3 的相关概念。

![Convergence of sqrt](handout-images/sqrt_graph.jpg "sqrt 在 0-3 区间的收敛特性，初值 1.0。")

**你需要完成：**

1. 编译运行 `sqrt`。汇报 ISPC 单核版（无 tasks）和多核版（有 tasks）相对串行的加速。分别给出 SIMD 并行带来的加速与多核并行带来的加速。
2. 修改输入数组内容，使 ISPC 相对串行加速 __最大化__。  
   报告无 tasks 与有 tasks 两种实现下的加速。你的修改是否提高了 SIMD 加速？是否提高了多核加速（即从无 tasks 到有 tasks 的收益）？说明原因。
3. 构造一种输入使 ISPC（无 tasks）相对串行的加速 __最小化__。  
   描述输入、设计理由和测得性能。效率损失原因是什么？  
   （注意当前 ISPC 使用 `--target=avx2`，即 8 路 SIMD）
4. _加分（最多 2 分）_：手写 AVX2 intrinsics 版本 `sqrt`，速度接近或超过 ISPC 产物可得分。可参考 [Intel Intrinsics Guide](https://software.intel.com/sites/landingpage/IntrinsicsGuide/)。

## Program 5：BLAS `saxpy`（10 分）

Program 5 实现了 BLAS 中的 saxpy：`result = scale * X + Y`。  
其中 `X`、`Y`、`result` 是长度为 `N` 的向量（此处 `N=20,000,000`），`scale` 为标量。  
每处理一个元素，约做两次算术（乘+加），访问三个数组元素。该计算“非常容易并行”，且访存模式规则。

**你需要完成：**

1. 编译运行 `saxpy`。程序会报告 ISPC（无 tasks）与 ISPC（有 tasks）性能。  
   观察并解释使用 tasks 的加速。你认为还能显著优化吗？（例如是否可能接近线性加速）请给出理由。
2. __加分（1 分）__：`main.cpp` 里总内存带宽写作 `TOTAL_BYTES = 4 * N * sizeof(float)`。  
   虽然每元素是读 X、读 Y、写 result 共 3 次访存，为什么乘数是 4 仍正确？（提示：考虑 CPU cache 行为）
3. __加分（酌情）__：继续优化 `saxpy`，要求明显提升，不是小幅百分点。若成功，请说明做法与理论上限，并欢迎和助教讨论。

注：这题历年常有人“想太多”。通常期望一个相对直接的解释，但如果你由此引出更多问题，欢迎来交流。

## Program 6：让 `K-Means` 更快（15 分）

Program 6 在 100 万数据点上运行 K-Means 聚类（[Wikipedia](https://en.wikipedia.org/wiki/K-means_clustering), [CS221 讲义](https://stanford.edu/~cpiech/cs221/handouts/kmeans.html)）。  
若你不熟算法细节也没关系：高层上它会从 K 个初始质心出发，迭代更新质心直到满足收敛条件。README 图示了初始与收敛状态（红星是质心，点颜色是簇分配）。

![K-Means starting and ending point](./handout-images/kmeans.jpg "K-Means 在二维可视化下的起止状态。")

起始代码功能正确，但速度不够理想。你的任务是找出 __哪里__ 慢、以及 __如何__ 优化。  
核心训练点是：__定位性能热点__。  
你不会被直接告知优化位置；你要先测量。第一反应应是“代码大部分时间花在哪”。请在源码中插入计时代码，依据测量结果聚焦主要瓶颈并优化。

**你需要完成：**

1. 在 `prog6_kmeans` 目录下创建数据集软链接：

   ```bash
   ln -s /afs/ir.stanford.edu/class/cs149/data/data.dat ./data.dat
   ```

   该数据较大（约 800MB），推荐软链接方式。若要本地拷贝，可在个人机器执行：

   ```bash
   scp [Your SUNetID]@myth[51-66].stanford.edu:/afs/ir.stanford.edu/class/cs149/data/data.dat ./data.dat
   ```

   有数据后编译并运行 `kmeans`（首次加载数据可能较慢），程序会输出总耗时。

2. 运行：

   ```bash
   pip install -r requirements.txt
   python3 plot.py
   ```

   这会根据 `kmeans` 生成的 `start.log` / `end.log` 画出 `start.png` / `end.png`。  
   你可能发现并非所有点都在二维投影下离自己质心最近，这是正常现象：原始数据是 100 维，绘图时通过 PCA 降到 2 维，会产生视觉偏差。  
   只要聚类整体看起来“合理”，并与起始代码结果相近即可。

3. 使用 `common/CycleTimer.h` 的 `CycleTimer::currentSeconds()`（返回秒）定位性能瓶颈：时间主要花在哪里？
4. 基于前一步优化实现。目标约 2.1x 或更高加速（`oldRuntime / newRuntime >= 2.1`）。  
   报告中要清晰写出推理链：测了什么 -> 推断了什么 -> 尝试了什么 -> 结果是加速/减速多少。

约束：

- 只能改 `kmeansThread.cpp`。不能改 `stoppingConditionMet`，不能改 `kMeansThread` 接口。其余可自由发挥（如扩展 `WorkerArgs`、重写函数、增减辅助数组等）。
- 必须保持算法功能不变：若不收敛，或 `plot.py` 结果与起始版本明显不一致，则实现有误。比如不能删主 while 循环，不能改变 `dist` 语义。
- __重要：__ 只能并行化以下四个函数之一：`dist`、`computeAssignments`、`computeCentroids`、`computeCost`。线程写法可参考 Program 1 的 `mandelbrotThread.cpp`。

提示：

- 本题代码改动通常不需要很多。课程参考解仅约 20~25 行增改。
- 定位热点后，请结合 `K`、`M`、`N` 的数量级来判断优化收益。
- 优先做“回报高”的优化，思考可利用的并行维度。
- 本题核心是训练 profiling/性能调试思维。即使没达到目标加速，只要报告体现扎实、合理的调试过程，也能拿到大部分分数。

## ARM Mac 怎么办？

如果你有 Apple ARM 新款笔记本，可参考 [README_aarch64.md](README_aarch64.md)。可额外产出一份 ARM 平台性能报告。助教也很关心你会观测到什么，尤其是 SIMD 加速表现。  
没有 ARM Mac 的同学可尝试云厂商 ARM 服务器（如 AWS），但课程组未充分测试。请不要把 ARM 平台代码提交到 Gradescope。

## 给好奇的你（强烈推荐）

想了解 ISPC 的来历？ISPC 两位作者之一 Matt Pharr 写过一篇非常精彩的文章：[The story of ispc](https://pharr.org/matt/blog/2018/04/30/ispc-all)。  
它涉及许多并行系统设计问题，尤其是“限定作用域语言 vs 通用编程语言”的价值，也回答了现实中的常见问题，比如“为什么编译器不能自动把我的程序并行化”。对 CS149 学生很值得一读。

## 提交说明

通过 [Gradescope](https://www.gradescope.com) 提交。  
小组作业只需一人提交，但请在 Gradescope 中添加队友。

需要在两个入口提交：

- `Assignment 1 (Write-Up)`
- `Assignment 1 (Code)`

`Assignment 1 (Write-Up)` 提交：

- 报告文件 `writeup.pdf`  
  （双人组需在文档中包含两人的姓名和 SUNet ID）

`Assignment 1 (Code)` 提交：

- Program 2 的 `main.cpp` 实现，命名为 `prob2.cpp`
- Program 6 的 `kmeansThread.cpp` 实现，命名为 `prob6.cpp`
- 其他附加代码（例如做了额外加分）

请在 write-up 中提醒助教关注你的加分项。提交代码必须能在 myth 机器上开箱编译运行。

## 资源与备注

- ISPC 文档与示例：<http://ispc.github.io/>
- Mandelbrot 图像的不同缩放区域很值得探索
- Intel AVX2 资料：<http://software.intel.com/en-us/avx/>
- [Intel Intrinsics Guide](https://software.intel.com/sites/landingpage/IntrinsicsGuide/) 非常有用

