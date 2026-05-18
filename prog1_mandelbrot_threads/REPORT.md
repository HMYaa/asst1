# Program 1 实验报告：基于线程的 Mandelbrot 并行化

## 1. 实验目的

1. 使用 `std::thread` 并行生成 Mandelbrot 图像，理解**多核线程级并行**。
2. 对比不同**工作划分策略**下的加速比，分析为何加速比往往**非线性**。
3. 通过 per-thread 计时定位负载不均（straggler），并实现一种适用于任意线程数的**静态负载均衡**划分，在 8 线程时接近硬件有效并行度（约 7–8×）。

## 2. 实验环境

| 项目 | 说明 |
|------|------|
| 代码目录 | `prog1_mandelbrot_threads/` |
| 图像尺寸 | 1600 × 1200 |
| 最大迭代 | 256 |
| 参考实现 | `mandelbrotSerial()`（串行） |
| 并行入口 | `mandelbrotThreadWithStrategy()` |

在 **Stanford myth** 机器（四核 Core i7，每核 2 超线程）上测量可获得满分要求的性能数据；本地编译可用于验证正确性。

## 3. 问题分析

### 3.1 计算特征

`mandelbrotSerial()` 对每一像素调用 `mandel()`，迭代次数取决于该点是否快速逃出 Mandelbrot 集合。**不同像素、不同行的计算代价差异很大**（输出亮度正比于代价）。

### 3.2 并行约束

- 各线程写入 `output` 的**不同行**，无数据竞争，**不需要锁**。
- 作业要求**禁止**线程间同步。
- `join()` 等待所有 worker 结束 → 总耗时由**最慢线程**决定（Amdahl / straggler 效应）。

### 3.3 两种静态划分

| 策略 | 做法 | 优点 | 缺点 |
|------|------|------|------|
| **块划分 (block)** | 连续行块分给各线程 | 实现简单；2 线程时即「上半/下半」 | 易把「重」区域集中在某一线程，负载不均 |
| **行交错 (interleaved)** | 线程 `t` 计算行 `t, t+N, t+2N, …` | 重/轻行打散，静态、无通信 | 每行一次 `mandelbrotSerial` 调用，有少量开销 |

## 4. 实现说明

### 4.1 代码结构

```
mandelbrotThread.h          — PartitionStrategy 枚举与 API
mandelbrotThread.cpp
  mandelbrotRowRange()      — 封装对 mandelbrotSerial 的调用
  computeBlockRange()       — 计算块划分的 startRow / numRows
  workerThreadStart_Block()           — 版本 A：连续行块
  workerThreadStart_Interleaved()     — 版本 B：行交错
  runWorker()               — 可选 per-thread 计时（-p）
  mandelbrotThreadImpl()    — 统一的 spawn / join 框架
  mandelbrotThreadWithStrategy()
  mandelbrotThread()        — 默认使用 interleaved
```

### 4.2 版本 A：块划分 (`block`)

对应作业步骤 1–2。在 `mandelbrotThread` 中为每个线程预计算：

```text
rowsPerThread = ⌈height / numThreads⌉
startRow[i]   = i × rowsPerThread
numRows[i]    = min(rowsPerThread, height - startRow[i])
```

Worker 调用一次 `mandelbrotRowRange(args, startRow, numRows)`。

### 4.3 版本 B：行交错 (`interleaved`)

对应作业步骤 4 的目标实现。Worker 循环：

```text
for j = threadId; j < height; j += numThreads
    mandelbrotRowRange(args, j, 1)
```

同一策略适用于 `numThreads = 2, 3, …, 8, 16`，无需按线程数硬编码。

### 4.4 Per-thread 计时（`-p`）

在 `runWorker()` 中统一计时，**block / interleaved 均可用**。块划分会额外打印负责的行区间 `rows [start, end)`，便于对照负载不均。

## 5. 运行与测试

### 5.1 编译

```bash
cd prog1_mandelbrot_threads
make
```

### 5.2 命令行

```bash
./mandelbrot -t <N> -v <1|2> -s <block|interleaved> [-p]
```

| 选项 | 含义 |
|------|------|
| `-t` | 线程数 |
| `-v` | 视图（1：默认；2：放大局部） |
| `-s` | 划分策略（默认 `interleaved`） |
| `-p` | 首轮运行打印各线程耗时（block 含行区间） |

### 5.3 建议实验序列

**正确性：** 任意策略、任意 `N` 应与串行输出一致（程序内 `verifyResult`）。

**步骤 1–2（块划分 + 加速比曲线）：**

```bash
for t in 2 3 4 5 6 7 8; do
  ./mandelbrot -t $t -v 1 -s block
done
```

记录输出的 `(X.XXx speedup from N threads)`，绘制 speedup–threads 曲线。

**步骤 3（每线程耗时，对比两种划分）：**

```bash
./mandelbrot -t 3 -v 1 -s block -p
./mandelbrot -t 3 -v 1 -s interleaved -p
```

**步骤 4（优化后 8 线程，两种视图）：**

```bash
./mandelbrot -t 8 -v 1 -s interleaved
./mandelbrot -t 8 -v 2 -s interleaved
```

**步骤 5（16 线程）：**

```bash
./mandelbrot -t 16 -v 1 -s interleaved
```

## 6. 预期结果与分析（待填入实测数据）

> 在 myth 上运行上述命令，将表格中的 `___` 替换为实测毫秒与加速比。

### 6.1 View 1：块划分 vs 行交错（8 线程）

| 策略 | 时间 (ms) | 相对串行加速比 |
|------|-----------|----------------|
| serial | ___ | 1.00× |
| block, 8 threads | ___ | ___× |
| interleaved, 8 threads | ___ | ___× |

**分析要点：**

- 块划分在 3 线程附近常出现**加速比回落**（某块落在高代价区域）。
- 行交错应明显更接近 **7–8×**（受 4 核 × 超线程限制，通常略低于 8）。

### 6.2 View 1：线程数扫描（interleaved）

| 线程数 | 时间 (ms) | 加速比 |
|--------|-----------|--------|
| 1 | ___ | 1.00× |
| 2 | ___ | ___× |
| 4 | ___ | ___× |
| 8 | ___ | ___× |
| 16 | ___ | ___× |

**16 线程：** 预期加速比**几乎不高于 8 线程**。原因：物理并行度约 8；过多线程带来调度与缓存竞争，`join` 仍等待最慢者。

### 6.3 Per-thread 计时（示例解读）

使用 `-p` 对比 `-s block` 与 `-s interleaved` 时，若块划分下各线程耗时差异大，而交错下更接近，即可验证：

- 非线性加速来自**负载不均**，而非 Mandelbrot 算法本身不可并行。
- `join` 同步点放大了 straggler 的影响。

## 7. 结论

1. Mandelbrot 逐像素代价不均，**连续行块**易导致 straggler，加速比随线程数**非线性**。
2. **行交错**在不引入同步的前提下，用单一静态策略平衡各线程工作量，是满足作业步骤 4 的简洁方案。
3. 8 线程附近接近 myth 机器有效并行上限；16 线程通常无法线性扩展。
4. 实现上通过 `PartitionStrategy` 与独立 worker 函数分离「框架」与「划分策略」，便于对比实验且保持可读性。

## 8. 提交说明

课程要求提交的写作为独立 `writeup.pdf`；本 `REPORT.md` 可作为撰写 PDF 的素材。代码提交仅需满足 Gradescope 对 Program 1 的要求（本程序无单独 prob1 文件，以仓库内实现为准）。
