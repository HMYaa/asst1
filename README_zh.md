# 作业 1：四核 CPU 上的性能分析

**截止：10 月 6 日（周一）23:59**

**总分 100 分 + 额外加分 6 分**

## 概述

本作业旨在帮助你理解现代多核 CPU 上两种主要的并行执行形式：

1. **单核内的 SIMD 执行**（单指令多数据）
2. **多核并行执行**（你还会观察到 Intel 超线程带来的影响）

你还将练习**测量并分析并行程序的性能**——这是贯穿整门课的重要能力，颇具挑战。本作业编程量不大，但需要做大量分析！

## 环境配置

**本作业需要在新的 myth 机器上运行代码**（主机名为 `myth[51-66].stanford.edu`）。若你因故无法在 myth 上拥有主目录，请通过 [HelpSU](https://stanford.service-now.com/it_services?id=sc_cat_item&sys_id=cab169801bd918d0685d4377cc4bcbe0) 提交工单。

这些机器配备四核 4.2 GHz Intel Core i7 处理器（在芯片认为合适且可行时，动态频率调节可提升至 4.5 GHz）。每个核心支持 **两个硬件线程**（Intel 称为「超线程」），并可执行 AVX2 向量指令——即在一条 **八宽 SIMD 指令**上，对八个单精度浮点数同时完成相同运算。若感兴趣，完整规格见  
<https://www.intel.com/content/www/us/en/products/sku/97129/intel-core-i77700k-processor-8m-cache-up-to-4-50-ghz/specifications.html>。想深入了解的同学可阅读 [Kaby Lake 微架构介绍](https://en.wikichip.org/wiki/intel/microarchitectures/kaby_lake)。

**说明：** 评分时我们要求你报告在 Stanford myth 机器上运行代码的性能；若出于兴趣，也可在本机运行本作业程序（需先安装 Intel SPMD Program Compiler (ISPC)，见 <http://ispc.github.io/>）。若在报告中包含其他机器上的结果，请务必明确说明运行环境。

### 入门步骤

**1. 安装 ISPC**

本作业中许多程序需要 ISPC 编译。在 myth 上可按以下步骤安装：

在 myth 机器上，将 Linux 二进制包下载到你选择的本地目录。可从 ISPC [下载页](https://ispc.github.io/downloads.html) 获取 Linux 版。在 `myth` 上建议用 `wget` 直接下载。截至 2025 年秋季第 1 周，以下命令可用：

```bash
wget https://github.com/ispc/ispc/releases/download/v1.28.1/ispc-v1.28.1-linux.tar.gz
```

解压：`tar -xvf ispc-v1.28.1-linux.tar.gz`

将 ISPC 的 `bin` 目录加入系统 `PATH`。例如解压到 `~/Downloads/ispc-v1.28.1-linux`，在 bash 中可执行：

```bash
export PATH=$PATH:${HOME}/Downloads/ispc-v1.28.1-linux/bin
```

可将上述行写入 `.bashrc` 以永久生效。

若使用 csh，需用 `setenv` 设置 `PATH`，可自行检索用法。

**2. 获取作业起始代码**

起始代码位于 <https://github.com/stanford-cs149/asst1>。请克隆仓库：

```bash
git clone https://github.com/stanford-cs149/asst1.git
```

---

## 程序 1：使用线程并行生成分形（20 分）

在代码库的 `prog1_mandelbrot_threads/` 目录中编译并运行（`make` 编译，`./mandelbrot` 运行）。程序会生成图像文件 `mandelbrot-serial.ppm`，即著名复数集合 **Mandelbrot 集** 的可视化。多数平台可查看 `.ppm` 文件。

若需远程查看图像，请先配置 **X 服务器**：Linux 通常无需额外安装；Mac 可用 [Xquartz](https://www.xquartz.org/)，Windows 可用 [VcXsrv](https://sourceforge.net/projects/vcxsrv/)。配置 SSH X 转发后，用 `ssh -Y` 登录 myth，即可用 `display` 命令查看图像。

如下所示，结果是熟悉而优美的分形。图像中每个像素对应复平面上的一个点；像素亮度与判定该点是否属于 Mandelbrot 集的计算代价成正比。要得到图 2，使用命令行选项 `--view 2`（参见 `mandelbrotSerial.cpp` 中的 `mandelbrotSerial()`）。更多定义见 <http://en.wikipedia.org/wiki/Mandelbrot_set>。

![Mandelbrot 集](handout-images/mandelbrot_viz.jpg "Mandelbrot 集两种视图的可视化。每个像素的计算代价与其亮度成正比。运行程序 1 和 3 时，可用命令行选项 `--view 2` 切换到视图 2。")

你的任务是使用 [std::thread](https://en.cppreference.com/w/cpp/thread/thread) 并行化图像计算。`mandelbrotThread.cpp` 中的 `mandelbrotThread()` 已提供可生成额外线程的起始代码：主线程通过 `std::thread(function, args...)` 创建线程，并通过 `join()` 等待其结束。当前被启动的线程尚未执行实际计算。你应在 `workerThreadStart` 中补充实现。本作业无需使用其他 `std::thread` API。

### 你需要完成的内容

1. **双线程并行：** 修改起始代码，用两个处理器并行生成 Mandelbrot 图像——线程 0 计算图像上半部分，线程 1 计算下半部分。这种按空间区域划分任务的方式称为 **空间分解**（spatial decomposition）。

2. **扩展至 2–8 线程：** 将工作按图像块分配给 2、3、4、5、6、7、8 个线程。处理器仅有四个物理核心，但每核支持两个超线程，因此最多可交错执行八个线程。在报告中，针对 **视图 1** 绘制相对参考串行实现的 **加速比** 随线程数变化的曲线。加速比是否随线程数线性增长？在报告中分析原因（也可为视图 2 绘图以辅助论证；提示：仔细查看三线程的数据点）。

3. **验证假设：** 在 `workerThreadStart()` 首尾插入计时代码，测量各线程完成工作所需时间。这些测量如何解释你在上一步得到的加速比曲线？

4. **改进工作划分：** 调整线程与工作的映射，使 Mandelbrot 两种视图的加速比均达到约 **7–8 倍**（超过 7 倍即可，不必苛求）。**不得在线程间使用同步。** 我们期望你设计一种对任意线程数都适用的静态划分策略——**禁止** 为每种配置硬编码不同方案（提示：存在非常简单的静态分配即可达成目标，且线程间无需通信/同步）。在报告中说明并行化思路，并给出 8 线程下的最终加速比。

5. **16 线程实验：** 用改进后的代码运行 16 个线程。性能是否明显优于 8 线程？为什么？

---

## 程序 2：使用 SIMD 内联函数向量化代码（20 分）

请阅读作业代码库 `prog2_vecintrin/main.cpp` 中的 `clampedExpSerial`。`clampedExp()` 对输入数组每个元素做 `values[i]` 的 `exponents[i]` 次幂，并将结果限制在 9.999999 以内。你的任务是将该代码向量化，以便在支持 SIMD 的机器上运行。

为降低难度，我们不要求你手写映射到真实 AVX2 指令的 SSE/AVX2 内联函数，而是使用 CS149 在 `CS149intrin.h` 中定义的 **「模拟向量内联函数」**。该库提供对向量值和向量掩码的操作（这些函数不会生成真实 CPU 向量指令，由库在软件中模拟，并便于调试）。`main.cpp` 中给出了向量化 `abs()` 的示例，包含基本的向量加载、存储与掩码操作。注意：`abs()` 示例较简单，且 **并未正确处理所有输入**（原因留给你自行发现）。建议通读 `CS149intrin.h` 中的注释与函数定义。

**实现提示：**

- 每条向量指令均可选掩码参数。掩码为 0 的通道被「屏蔽」，该通道不会被本次向量运算结果覆盖。未指定掩码时，等价于全 1 掩码。
- 提示：本题的解法会用到多个掩码寄存器及库中的掩码操作。
- 提示：`_cs149_cntbits` 可能有用。
- 若循环总迭代次数不是 SIMD 向量宽度的整数倍会怎样？建议用 `./myexp -s 3` 测试。提示：`_cs149_init_ones` 可能有用。
- 提示：用 `./myexp -l` 可在程序结束时打印已执行的向量指令日志；可用 `addUserLog()` 添加自定义调试信息，也可用 `CS149Logger.printLog()`。

程序会检查输出是否正确。若有误，会打印首个错误及输入输出对照表。你的结果在 `output =` 之后，应与 `gold =` 之后一致。程序还会打印 CS149 模拟向量单元的利用率统计。**性能以「Total Vector Instructions」为准**（可假设每条模拟向量指令在模拟 SIMD CPU 上耗时 1 个周期）。「Vector Utilization」表示启用的向量通道占比。

### 你需要完成的内容

1. 在 `clampedExpVector` 中实现 `clampedExpSerial` 的向量化版本，需支持任意输入规模 `N` 与向量宽度 `VECTOR_WIDTH`。

2. 运行 `./myexp -s 10000`，在 `CS149intrin.h` 中将 `VECTOR_WIDTH` 依次设为 2、4、8、16，记录向量利用率。随 `VECTOR_WIDTH` 变化，利用率是升高、降低还是不变？说明原因。

3. **额外加分（1 分）：** 在 `arraySumVector` 中实现 `arraySumSerial` 的向量化版本。可假设 `VECTOR_WIDTH` 整除 `N`。串行实现为 `O(N)`，你的实现应力争达到 `O(N / VECTOR_WIDTH + VECTOR_WIDTH)`，或更优的 `O(N / VECTOR_WIDTH + log2(VECTOR_WIDTH))`。`hadd` 与 `interleave` 可能有用。

---

## 程序 3：使用 ISPC 并行生成分形（20 分）

在熟悉 SIMD 之后，我们回到 Mandelbrot 分形并行（与程序 1 类似）。程序 3 同样计算 Mandelbrot 图像，但通过同时利用 CPU 的四个核心以及每个核心内的 SIMD 执行单元，可获得更高加速比。

程序 1 为每个处理核心创建一个线程，并将部分计算分配给这些并发线程（线程与核心一一对应，相当于显式把任务绑到核心上）。程序 3 则使用 ISPC 语言构造描述 **彼此独立的计算**；这些计算可并行执行而不破坏程序正确性（且确实会并行执行）。对 Mandelbrot 图像而言，每个像素的计算相互独立。ISPC 编译器与运行时负责生成尽可能高效利用 CPU 并行资源的程序。

你需要对程序 3（由 C++ 与 ISPC 混合编写）做一处简单修改（该问题影响性能而非正确性）。修正后，相对 `mandelbrotSerial()` 的原始串行实现，性能应提升 **超过 32 倍**。

### 程序 3，第一部分：ISPC 基础（20 分中的 10 分）

阅读 ISPC 代码时须牢记：尽管语法类似 C/C++，其执行模型与标准 C/C++ 不同。与 C 不同，ISPC 程序的多个 **程序实例**（program instances）总是在 CPU 的 SIMD 单元上并行执行。并发实例数量由编译器根据目标机器选定，程序员可通过内建变量 `programCount` 查询；当前实例编号为 `programIndex`。因此，从 C 调用 ISPC 函数可理解为启动一组并发 ISPC 实例（文档中称为 **gang**）；gang 运行完毕后控制权返回 C 代码。

**请停一下。这是友善的授课教师在说：请把上一段再读一遍。相信我。**

例如，下列程序用 C 与 ISPC 相加两个长度为 1024 的向量。如课堂所讲，gang 中各实例相互独立、执行相同逻辑，可通过 SIMD 指令加速。

**C 代码（`myprogram.cpp`）：**

```c
const int TOTAL_VALUES = 1024;
float a[TOTAL_VALUES];
float b[TOTAL_VALUES];
float c[TOTAL_VALUES]

// 在此初始化 a、b

sum(TOTAL_VALUES, a, b, c);

// 返回后，c 中存放 a + b 的结果
```

**对应 ISPC 代码（`myprogram.ispc`）：**

```c
export sum(uniform int N, uniform float* a, uniform float* b, uniform float* c)
{
  // 假设 programCount 整除 N
  for (int i=0; i<N; i+=programCount)
  {
    c[programIndex + i] = a[programIndex + i] + b[programIndex + i];
  }
}
```

上述 ISPC 代码在多个程序实例间交错处理数组元素，与程序 1 中静态划分图像区域的做法类似。

然而，与其思考如何把任务映射到执行单元，往往更清晰、也更有力的是 **只关注如何将问题划分为相互独立的部分**。ISPC 的 `foreach` 构造即可表达这种分解。下面 `sum2` 中的 `foreach` 定义了迭代空间，其中各次迭代相互独立、可任意顺序执行；ISPC 负责将迭代分配给并发实例。`sum` 与 `sum2` 的差别微妙但至关重要：`sum` 是 **命令式** 的（描述如何把任务映射到实例），`sum2` 是 **声明式** 的（只说明要做哪些工作）：

```c
export sum2(uniform int N, uniform float* a, uniform float* b, uniform float* c)
{
  foreach (i = 0 ... N)
  {
    c[i] = a[i] + b[i];
  }
}
```

继续之前，建议阅读 ISPC 教程：<http://ispc.github.io/example.html>。教程中的示例与程序 3 中 `mandelbrot.ispc` 的 `mandelbrot_ispc()` 几乎相同；作业代码中我们调整了 `foreach` 的边界，使实现更直观。

### 你需要完成的内容（第一部分）

1. 编译并运行 `mandelbrot_ispc`。**当前 ISPC 编译器配置为生成 8 宽 AVX2 向量指令。** 根据你对这些 CPU 的了解，理论上最大加速比是多少？实际观测值为何可能低于理想值？（提示：考虑你所做计算的特性；图像哪些区域对 SIMD 更具挑战？比较不同视图的渲染性能有助于验证假设。）

   提醒：本节所述代码中，ISPC 将 gang 映射到 **单核** 上的 SIMD 指令；这与程序 1 通过多核多线程获得加速的方式不同。

若查阅 myth 机器 CPU 的详细技术资料，会发现标量与向量指令每周期可发射数量规则相当复杂。本作业中，可近似认为 **浮点运算的 8 宽向量执行单元数量与标量执行单元相当**。

### 程序 3，第二部分：ISPC 任务（20 分中的 10 分）

ISPC 的 SPMD 模型与 `foreach` 等机制便于编写利用 SIMD 的程序；语言还提供 **ISPC 任务**（tasks）以利用多核。

参见 `mandelbrot_ispc_withtasks` 中的 `launch[2]`：启动两个任务，每个任务由一组 ISPC 程序实例执行，在 `mandelbrot_ispc_task` 中各计算最终图像的一个区域。与 `foreach` 将可任意顺序（且由实例并行）执行的循环迭代类似，`launch` 创建的任务也可任意顺序、在不同 CPU 核心上并行处理。

### 你需要完成的内容（第二部分）

1. 带参数 `--tasks` 运行 `mandelbrot_ispc`。在视图 1 上观测到的加速比是多少？相对未划分任务的 `mandelbrot_ispc` 版本又如何？

2. 仅修改 `mandelbrot_ispc_withtasks()`，通过调整创建的任务数量，可使 `mandelbrot_ispc --tasks` 的性能 **超过串行版本 32 倍以上**。你如何确定任务数量？为何该数量效果最佳？

3. **额外加分（2 分）：** 比较程序 1 的线程抽象与 ISPC 任务抽象有何异同？除 `(create/join)` 与 `(launch/sync)` 在语义上的明显差别外，深层含义更微妙。思考实验：启动 10,000 个 ISPC 任务会怎样？启动 10,000 个线程呢？（请讨论一般情况，不必拘泥于本 Mandelbrot 程序。）

**爱思考的同学可能会问：** 为何要用两种机制（`foreach` 与 `launch`）表达可并行、相互独立的工作？系统不能把 `foreach` 的迭代分到所有核心上，再为各核心生成合适 SIMD 代码吗？

**答：** 好问题！答案有很多可能，欢迎来 office hours 讨论。

---

## 程序 4：迭代法求 `sqrt`（15 分）

程序 4 是用 ISPC 对 2000 万个 [0, 3] 区间内的随机数求平方根。它采用基于牛顿法的快速迭代实现，求解方程 $\frac{1}{x^2} - S = 0$，初值取 1.0。下图展示在该区间内，不同输入达到准确解所需的迭代次数（区间外不收敛）。可见收敛速度取决于初值好坏。

**说明：** 本题用于复习，所涉概念与程序 2、3 类似。

![sqrt 收敛曲线](handout-images/sqrt_graph.jpg "初值 1.0 时，[0,3] 区间内 sqrt 的收敛迭代次数。输入为 1 时立即收敛；趋近 0 或 3 时迭代增多（输入为 3 时最多）。")

### 你需要完成的内容

1. 编译并运行 `sqrt`。报告 ISPC 在单核（无 tasks）与全核（有 tasks）下的加速比。分别来自 SIMD 与多核并行的加速是多少？

2. 修改 `values` 数组内容，改善 ISPC 相对串行版本的加速比。构造一种输入，使 **相对串行版本的加速比最大**，并报告有/无 tasks 两种 ISPC 实现的结果。你的修改是否提高了 SIMD 加速？是否提高了多核加速（从无 tasks 到有 tasks）？请解释原因。

3. 构造一种输入，使 **ISPC（无 tasks）相对串行的加速比最小**。描述该输入、选择理由及 ISPC 相对性能。效率损失的原因是什么？（请记住 ISPC 使用 `--target=avx2`，生成 8 宽 SIMD 指令。）

4. **额外加分（最多 2 分）：** 用手写 AVX2 内联函数实现自己的 `sqrt`，性能应接近或优于 ISPC 生成的二进制。可参考 [Intel Intrinsics Guide](https://software.intel.com/sites/landingpage/IntrinsicsGuide/)。

---

## 程序 5：BLAS `saxpy`（10 分）

程序 5 实现 BLAS（基本线性代数子程序）库中广泛使用的 `saxpy`：`result = scale*X + Y`，其中 `X`、`Y`、`result` 为长度 `N` 的向量（本程序中 `N` = 2000 万），`scale` 为标量。`saxpy` 每使用三个元素完成两次运算（一次乘法、一次加法），属于 **极易并行** 的计算，具有可预测的规则访存与执行代价。

### 你需要完成的内容

1. 编译并运行 `saxpy`。程序会报告 ISPC（无 tasks）与 ISPC（有 tasks）的性能。使用 tasks 带来多少加速？解释该程序的性能表现。你认为还有大幅提升空间吗？（例如能否改写代码以接近线性加速？请回答是或否并论证。）

2. **额外加分（1 分）：** `main.cpp` 中总内存带宽按 `TOTAL_BYTES = 4 * N * sizeof(float)` 计算。尽管 `saxpy` 各从 `X`、`Y` 读一个元素、向 `result` 写一个元素，为何乘数 4 仍然正确？（提示：考虑 CPU 缓存。）

3. **额外加分（按情况给分）：** 提升 `saxpy` 性能。我们期望看到 **显著** 加速，而非几个百分点。若成功，请说明做法及在这些系统上理论最优性能；也欢迎来找助教展示成果。

**说明：** 往年有同学在此题上想得太复杂。我们期望简洁的回答；若运行结果引发更多疑问，欢迎与助教交流。

---

## 程序 6：加速 K-Means（15 分）

程序 6 使用 K-Means 算法（[维基百科](https://en.wikipedia.org/wiki/K-means_clustering)、[CS 221 讲义](https://stanford.edu/~cpiech/cs221/handouts/kmeans.html)）对一百万个数据点聚类。算法从 `K` 个初始质心出发，迭代更新直至满足收敛条件。下图展示算法开始与结束时的状态：红星为质心，点的颜色表示所属簇。

![K-Means 起止状态](./handout-images/kmeans.jpg "二维数据上 K-Means 算法的起始与结束状态。")

起始代码中的 K-Means 实现 **正确**，但还不够快。你的任务是找出 **何处** 需要优化以及 **如何** 优化——本程序练习的是 **定位性能热点**。我们不会告诉你该改哪里；你应先用计时代码测量，找出占用大部分时间的路径，再深入分析能否加速。

### 你需要完成的内容

1. 在 `prog6_kmeans` 目录下执行  
   `ln -s /afs/ir.stanford.edu/class/cs149/data/data.dat ./data.dat`  
   创建指向数据集的符号链接（约 800MB，推荐此方式）。若需本地副本，可在个人机器上执行：  
   `scp [你的 SUNetID]@myth[51-66].stanford.edu:/afs/ir.stanford.edu/class/cs149/data/data.dat ./data.dat`  
   然后编译运行 `kmeans`（首次加载数据可能较慢）。程序会报告算法总运行时间。

2. 运行 `pip install -r requirements.txt` 安装绘图依赖，再运行 `python3 plot.py`，根据 `kmeans` 生成的 `start.log`、`end.log` 得到 `start.png`、`end.png`，应与上文图示类似。**注意：** 你可能发现并非所有点都分配给「最近」的质心——这是正常的。（原因：我们将 100 维数据用 [PCA](https://en.wikipedia.org/wiki/Principal_component_analysis) 投影到 2 维可视化，高维空间中靠近某质心的点，投影后未必在 2 维上靠近该质心。）只要聚类结果「合理」（可参考第 2 步起始代码生成的图），且多数点似乎分配给最近质心，即视为正确。

3. 使用 `common/CycleTimer.h` 中的 `CycleTimer::currentSeconds()`（返回秒为单位的浮点数）定位瓶颈。时间主要花在哪里？

4. 根据测量结果改进实现，目标加速比约 **2.1 倍及以上**（即 $\frac{oldRuntime}{newRuntime} \geq 2.1$）。在报告中按步骤说明：测量 → 假设 → 尝试 → 加速比/减速比。期望类似：「我测量了……，因此认为 X；我尝试……，得到……倍加速/减速」。

**约束：**

- 仅可修改 `kmeansThread.cpp`。不得修改 `stoppingConditionMet`，不得改变 `kMeansThread` 的接口；但可扩展 `WorkerArgs`、重写函数、分配新数组等。
- **不得改变算法语义！** 若算法不收敛，或 `python3 plot.py` 结果与起始代码差异很大，则有问题。例如不能简单删掉主 `while` 循环或改变 `dist` 的语义。
- **重要：** 你只能并行化以下函数中的 **一个**：`dist`、`computeAssignments`、`computeCentroids`、`computeCost`。并行写法可参考 `prog1_mandelbrot_threads/mandelbrotThread.cpp`。

**提示：**

- 本题不需要大量代码；参考解法约修改/新增 20–25 行。
- 用计时器定位热点后，请理解 `K`、`M`、`N` 的相对规模。
- 优先改动收益高的部分，并思考问题中可用的并行维度。
- **本题旨在练习性能剖析与调试；即使未达性能目标，只要在报告中展现清晰的分析过程，仍可获得大部分分数。**

---

## 关于 ARM 架构 Mac

若可使用新款 Apple ARM 笔记本，请参阅 [此处说明](README_aarch64.md)，报告各程序在 ARM 机器上的性能。助教对你们的发现很感兴趣：SIMD 带来多少加速？没有新款 Mac 的同学也可尝试云上的 ARM 服务器（助教未充分测试）。**请勿将 ARM 机器上运行的代码提交到 Gradescope。**

---

## 拓展阅读（强烈推荐）

想了解 ISPC 及其诞生过程？ISPC 的两位作者之一 Matt Pharr 撰写了精彩博文 [The story of ispc](https://pharr.org/matt/blog/2018/04/30/ispc-all)，涉及许多并行系统设计议题——尤其是专用语言与通用语言的取舍，以及「为何编译器不能自动把我的程序并行化？」等现实问题。CS149 同学值得一读！

---

## 提交说明

通过 [Gradescope](https://www.gradescope.com) 提交。每组只需提交一次，但请在 Gradescope 上注明组员姓名。需提交至两处：`Assignment 1 (Write-Up)` 与 `Assignment 1 (Code)`。

**Assignment 1 (Write-Up)：**

- 报告文件 `writeup.pdf`，须包含两位组员的姓名与 SUNet ID（若为两人一组）。

**Assignment 1 (Code)：**

- 程序 2 的 `main.cpp` 实现，文件名为 `prob2.cpp`
- 程序 6 的 `kmeansThread.cpp` 实现，文件名为 `prob6.cpp`
- 其他代码（例如额外加分部分）

若有额外加分，请在报告中说明，以便助教查阅。提交时，所有代码须能在 myth 机器上 **直接编译运行**，无需额外配置。

---

## 参考资料

- ISPC 文档与示例：<http://ispc.github.io/>
- 放大 Mandelbrot 图像不同区域往往十分有趣
- Intel 关于 AVX2 的文档：<http://software.intel.com/en-us/avx/>
- [Intel Intrinsics Guide](https://software.intel.com/sites/landingpage/IntrinsicsGuide/) 非常实用
