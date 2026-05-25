# Program 6: K-Means Optimization Report

## 作业目标

本题要求在不改变 K-Means 结果语义的前提下，加速 `prog6_kmeans/kmeansThread.cpp` 中的实现。核心不是盲目改代码，而是先定位运行时间主要花在哪里，再选择一个收益最高、风险最低的函数并行化。题目约束包括：

- 只能修改 `kmeansThread.cpp`。
- 不能修改 `stoppingConditionMet`，不能改变 `kMeansThread` 的接口。
- 不能改变 K-Means 的收敛语义、距离语义或最终聚类行为。
- 只能在 `dist`、`computeAssignments`、`computeCentroids`、`computeCost` 中选择一个函数并行化。

## 需要掌握的知识点

K-Means 每轮迭代包含三步：

1. `computeAssignments`：对每个数据点，计算它到所有质心的距离，选择最近质心。
2. `computeCentroids`：根据 assignment，把每个簇中所有点求平均，更新质心。
3. `computeCost`：计算每个簇的距离代价，用于判断是否收敛。

如果有 `M` 个点、每个点 `N` 维、`K` 个质心，则每轮复杂度大致是：

- assignment：`O(M * K * N)`
- centroid update：`O(M * N)`
- cost：`O(M * N)`

本题数据规模中 `M` 很大、`N=100`、`K` 相对很小但仍大于 1，因此 assignment 需要做最多的距离计算，是最自然的优化入口。

## 第一性原理分析

并行化的关键不是“哪里有循环就开线程”，而是找到可以独立执行、共享写入最少的维度。

`computeAssignments` 的本质是：

```text
for each point m:
    best = argmin_k distance(data[m], centroid[k])
    assignment[m] = best
```

每个点的最近质心只依赖只读的 `data` 和 `clusterCentroids`，最后只写自己的 `clusterAssignments[m]`。因此按点 `m` 切分时，不同线程写入不重叠，没有数据竞争，也不需要锁。

相反，如果按质心 `k` 切分，不同线程会同时尝试更新同一个点的最小距离和 assignment，需要额外同步或 per-thread 临时数组再归约；这会增加复杂度，也容易改变语义。因此本实现选择按数据点切分。

## 实现

我将 `computeAssignments` 改为处理 `[start, end)` 范围内的数据点。每个点内部仍按 `k = 0..K-1` 顺序扫描所有质心，使用原来的 `dist` 函数，因此 tie-breaking 行为和距离语义保持一致。

在 `kMeansThread` 主循环中，每轮创建 8 个线程，把 `M` 个点均匀分块，只并行执行 `computeAssignments`。`computeCentroids` 和 `computeCost` 保持串行，避免引入共享累加的锁或归约逻辑，也符合题目“只并行化一个函数”的限制。

## 测量结果

本地没有官方 `data.dat`，因此我用临时 harness 生成确定性合成数据验证。参数为 `M=200000, N=100, K=3, epsilon=0.1`。

| Version | Time | Centroid checksum | Assignment checksum |
| --- | ---: | ---: | ---: |
| Starter | 7025.342 ms | 22869.659526430529 | 17961532059 |
| Optimized | 3045.309 ms | 22869.659526430529 | 17961532059 |

加速比：

```text
7025.342 / 3045.309 = 2.31x
```

checksum 完全一致，说明在该确定性输入上最终质心和 assignment 未改变。

我还用更接近作业规模的合成输入 `M=1000000, N=100, K=3, epsilon=0.1` 运行优化后实现，耗时为 `6563.647 ms`。由于本地缺少官方数据文件，这个结果只用于确认大输入下实现可运行；最终报告官方数据时应以 `prog6_kmeans/data.dat` 的实际运行时间为准。

## 结论

优化出发点是 assignment 阶段的 `O(M*K*N)` 主导开销，以及按数据点切分天然无共享写冲突。最终实现没有改变算法语义，只把最大热点并行化，达到了约 `2.31x` 的本地合成数据加速。
