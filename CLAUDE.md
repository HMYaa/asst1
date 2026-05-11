# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 概述

CS149 作业 1：四核 CPU 性能分析。本作业探索现代 CPU 上的两种并行执行形式：单核内的 SIMD 执行，以及使用线程的多核并行执行。

**总分**：100 分 + 6 分额外加分。

**目标平台**：Stanford myth 机器（myth[51-66].stanford.edu）
- CPU：4 核 Intel i7-7700K 处理器（4.2 GHz，AVX2 8 路 SIMD）
- 每核通过超线程支持 2 个硬件线程，共 8 个逻辑核心
- ISPC 编译目标：`--target=avx2-i32x8 --arch=x86-64 --opt=disable-fma`

## 构建命令

每个程序有自己的 Makefile。在各程序目录下构建：

```bash
cd prog1_mandelbrot_threads && make    # 基于线程的 mandelbrot (C++11, -O3)
cd prog2_vecintrin && make             # 向量内联函数 (CS149 模拟 SIMD)
cd prog3_mandelbrot_ispc && make       # 基于 ISPC 的 mandelbrot (ISPC -> avx2-i32x8)
cd prog4_sqrt && make                  # ISPC sqrt (-march=native)
cd prog5_saxpy && make                 # ISPC saxpy (ISPC -> avx2-i32x8)
cd prog6_kmeans && make                # K-Means 优化 (-march=native)
```

清理构建产物：在各程序目录下执行 `make clean`（删除 objs/、*.ppm、*.log、可执行文件）

仓库根目录提供了 `compile_commands.json` 用于 LSP/IDE 支持（VSCode 通过 `.vscode/settings.json` 读取）。

### ISPC 编译流程

prog3-5 的 `.ispc` 文件通过以下规则编译：
```bash
$(ISPC) $(ISPCFLAGS) $< -o $(OBJDIR)/$*_ispc.o -h $(OBJDIR)/$*_ispc.h
```
- 生成对象文件：`objs/<name>_ispc.o`
- 生成 C++ 头文件：`objs/<name>_ispc.h`（包含 ISPC 函数的声明）
- C++ 代码通过 `#include "objs/mandelbrot_ispc.h"` 调用 ISPC 函数

## 运行与性能分析

每个程序生成可通过 `./<executable_name>` 运行的可执行文件：

- **prog1**：`./mandelbrot`（选项：`--view 1|2`，`--threads N`）
- **prog2**：`./myexp`（选项：`-s N` 指定数组大小，`-l` 打印指令日志）
- **prog3**：`./mandelbrot_ispc`（选项：`--view 1|2`，`--tasks N` 启用多核任务）
- **prog4**：`./sqrt`
- **prog5**：`./saxpy`（选项：`--tasks N`）
- **prog6**：`./kmeans`（需要 `data.dat` 符号链接，见下文）

**性能分析**：使用 `common/CycleTimer.h` 中的 `CycleTimer::currentSeconds()` 进行计时。返回 `double` 类型的秒数。

计时代码示例：
```cpp
#include "CycleTimer.h"
// ...
double startTime = CycleTimer::currentSeconds();
// ... 要测量的代码 ...
double endTime = CycleTimer::currentSeconds();
printf("耗时: %f 秒\n", endTime - startTime);
```

在函数起始/结束处插入以测量运行时长。这是本作业中性能分析的标准方法。

**查看 PPM 图像**（在支持 X11 的 myth 机器上）：`ssh -Y user@mythXX` 然后 `display mandelbrot-serial.ppm`。在 Linux 本地机器上，大多数图像查看器可直接打开 PPM 文件。

### 验证正确性

- **prog2**：程序自动验证输出正确性，会打印首个错误及输入输出对照表。"output = "应与 "gold = " 匹配。同时输出向量利用率统计。
- **prog3**：需修复的是性能问题（非正确性），修复前后输出应一致。
- **prog6**：若 `python3 plot.py` 生成的聚类图与 starter code 差异明显，或算法不收敛，则实现有误。不必所有点都分配给最近的质心（PCA 投影导致）。即使未达性能目标，只要在 writeup 中展示良好的调试技能，仍可获大部分分数。

## 架构

### 关键约束

- **prog1**（20分）：只能修改 `mandelbrotThread.cpp`。不允许线程间同步。使用 `std::thread` 配合 `join()`。
  - 主线程也作为 worker 参与计算（`workerThreadStart(&args[0])`），只创建 `numThreads-1` 个新线程
  - 目标：8线程达到约 7-8x 加速（两个视图）
  - 需要单一工作分解策略适用于所有线程数（不能硬编码）
  - 在 `workerThreadStart` 中插入计时代码分析各线程耗时
  - `WorkerArgs` 结构体包含 `threadId` 和 `numThreads`，用于静态分配工作
- **prog2**（20分）：只能修改 `main.cpp`（以 `prob2.cpp` 提交）。使用模拟的 `CS149intrin.h` 库——不是真实的 SIMD。向量宽度通过 CS149intrin.h 中的 `#define VECTOR_WIDTH` 设置。测试边界情况：`./myexp -s 3`（N 不是向量宽度的倍数）。使用 `./myexp -l` 打印指令日志，可用 `addUserLog()` 添加自定义调试信息。
- **prog3-5 (ISPC)**：ISPC 文件（.ispc）通过 `ispc --target=avx2-i32x8 --arch=x86-64 --opt=disable-fma` 编译为对象文件和头文件。多核并行通过 `launch[N]` 任务实现。prog3 需修复一个性能问题（非正确性问题），修复后应超过 32x 加速。
- **prog6**（15分）：只能修改 `kmeansThread.cpp`（以 `prob6.cpp` 提交）。
  - **只能并行化恰好一个函数**：`dist`、`computeAssignments`、`computeCentroids` 或 `computeCost`
  - 不能修改 `stoppingConditionMet` 函数，不能改变 `kMeansThread` 接口
  - 目标：2.1x 或更多加速
  - 提示：解决方案约 20-25 行代码改动，需理解 K（聚类数）、M（点数）、N（维度数）的相对大小
  - 数据文件：`cd prog6_kmeans && ln -s /afs/ir.stanford.edu/class/cs149/data/data.dat ./data.dat`
  - 绘图验证：`pip install -r requirements.txt && python3 plot.py`
  - `WorkerArgs` 结构体包含 `start` 和 `end` 字段用于工作分配

### 共享代码 (common/)

- `CycleTimer.h` - 计时工具
- `ppm.cpp` - PPM 图像输出辅助函数
- `tasksys.cpp` - ISPC 任务系统实现（为 ISPC 程序提供 `launch`/`sync`）

## ISPC 环境配置

ISPC 必须安装并加入 PATH。从 <http://ispc.github.io/downloads.html> 下载：

```bash
wget https://github.com/ispc/ispc/releases/download/v1.28.1/ispc-v1.28.1-linux.tar.gz
tar -xvf ispc-v1.28.1-linux.tar.gz
export PATH=$PATH:${HOME}/Downloads/ispc-v1.28.1-linux/bin
```

## 提交说明

- 每个小组只需提交一次，但需在 Gradescope 中添加伙伴的姓名和 SUNet ID。
- **实验报告**：`writeup.pdf`（包含两人姓名和 SUNet ID）通过 Gradescope `Assignment 1 (Write-Up)` 提交。
- **代码**：`prob2.cpp`（即 prog2 的 `main.cpp`）和 `prob6.cpp`（即 prog6 的 `kmeansThread.cpp`）通过 Gradescope `Assignment 1 (Code)` 提交。额外加分请在 writeup 中告知评分助理。
- 提交的代码必须能在 myth 机器上直接编译运行。

## 注意事项

- 对于 ARM 架构的 Mac，参见 `README_aarch64.md` 获取平台特定说明。
- 额外加分机会：prog2（向量化 `arraySum`）、prog3（ISPC 任务与线程对比讨论）、prog4（AVX2 内联函数实现 sqrt）、prog5（saxpy 优化）。
- prog6 数据规模：100 万点、100 维，数据文件约 800MB。首次运行加载数据可能较慢。

## 参考资源

- ISPC 文档与示例：<http://ispc.github.io/>、<http://ispc.github.io/example.html>
- Intel Intrinsics Guide：<https://software.intel.com/sites/landingpage/IntrinsicsGuide/>
- ISPC 开发故事：<https://pharr.org/matt/blog/2018/04/30/ispc-all>
- K-Means 算法：<https://en.wikipedia.org/wiki/K-means_clustering>
