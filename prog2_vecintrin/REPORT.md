# Program 2：SIMD 向量化实验报告

## 1. 作业目标

| 任务 | 说明 |
|------|------|
| **必做** `clampedExpVector` | 向量化 `clampedExpSerial`：计算 `values[i]^exponents[i]` 并 clamp 到 9.999999 |
| **必做** 利用率分析 | `./myexp -s 10000`，`VECTOR_WIDTH` = 2/4/8/16，记录并解释 Vector Utilization |
| **加分** `arraySumVector` | 向量化数组求和；`N % VECTOR_WIDTH == 0`，目标约 `O(N/W + log W)` |

**验收指标**

- 正确性：`output` 与串行 `gold` 一致（误差约 `1e-5`）
- 性能：**Total Vector Instructions**（越少越好）；**Vector Utilization** = 活跃 lane 数 / 总 lane 数

---

## 2. 第一性原理与设计要点

### 2.1 SIMD 在做什么

- 一条向量指令同时更新 `VECTOR_WIDTH` 个 **lane**，前提是各 lane 走**同一条指令流**。
- **Mask**：为 0 的 lane 不写入结果，用来表达 `if/while` 而不拆成标量循环。

### 2.2 `clampedExp` 的串行语义

```text
若 exponent == 0 → output = 1
否则 result = x，重复 (exponent - 1) 次 result *= x
若 result > 9.999999 → result = 9.999999
```

向量化难点：

1. **指数 per-lane 不同** → `while (count > 0)` 是 **lane 级分歧**
2. **exponent == 0** 需单独 mask
3. **N 不一定整除 W** → 尾部用 `_cs149_init_ones(rem)`

### 2.3 优化出发点（不是表面改循环）

| 原则 | 做法 |
|------|------|
| 正确性优先 | 尾部 mask、`exp==0` 写 1、`count` 只在需要乘的 lane 上递减 |
| 用 mask 表达分歧 | `count > 0` 时 masked `vmult` / `vsub`，避免标量回退 |
| 有界迭代 | `exponent < EXP_MAX(10)` → 最多 `EXP_MAX-1` 次乘，可用**固定次数**循环去掉 `cntbits` |
| 减少冗余指令 | `count` 由 `exp` 移动再减 1，省掉 `vset_int(count,0)` |

---

## 3. 实现版本（逐步优化）

代码在 `main.cpp` 的 TODO 区域，通过宏切换版本（不改框架其它部分）：

```cpp
#ifndef CLAMPED_EXP_VERSION
#define CLAMPED_EXP_VERSION 3   // 默认最优
#endif
```

### 版本 1：基础向量化 + 动态 `while`

**思路**：外层 `i += VECTOR_WIDTH`；内层 `while (_cs149_cntbits(count > 0))` 做 masked 乘方。

**优点**

- 指数小时提前退出，内层迭代次数少
- 利用率统计上可能更高（本机 v1 约 **80.7%**）

**缺点**

- 每次循环调用 `_cs149_cntbits`（额外向量指令）
- 总指令数最多（本机 N=10000, W=4：**97075** 条）

### 版本 2：固定 `EXP_MAX-1` 次迭代

**思路**：用 `for (iter = 0; iter < EXP_MAX - 1; iter++)` 替代 `while`；每轮 `maskActive = (count > 0)` 再 `vmult` / `vsub`。

**优点**

- 去掉 `cntbits`，总指令数下降（**95003**，约少 2.1%）
- 控制流简单，易维护

**缺点**

- 指数小时仍跑满 9 轮 → **Vector Utilization 降至约 66.9%**

### 版本 3：固定迭代 + 精简 `count` 初始化（默认）

**相对 v2 的改动**：用 `vmove_int(count, exp)` + masked `vsub 1`，替代 `vset 0` + `vsub(exp,1)`。

- `exp == 0` 的 lane：`count` 保持 0，不进入乘法门
- 指令数与 v2 相同（**95003**），代码更短

**切换方式**

```bash
# 编译时指定版本
g++ -I../common -DCLAMPED_EXP_VERSION=1 logger.o CS149intrin.o main.cpp -o myexp
```

### 版本对比（本机，`N=10000`，`VECTOR_WIDTH=4`）

| 版本 | Total Vector Instructions | Vector Utilization | 说明 |
|------|---------------------------|--------------------|------|
| v1 | 97075 | 80.7% | 动态 while，指令多、利用率高 |
| v2 | 95003 | 66.9% | 固定 9 轮，指令最少类之一 |
| **v3** | **95003** | **66.9%** | **默认**；同 v2 性能，代码更简 |

**结论**：作业以 **Total Vector Instructions** 计性能时，选 **v3**；若只关心利用率数字，v1 更高但以更多指令为代价。

### 3.1 为什么 v2 / v3 的 Vector Utilization 更低？

先明确统计方式（见 `logger.cpp`）：

```text
Vector Utilization = Utilized Vector Lanes / Total Vector Lanes
                 = 所有向量指令上「mask 为 1 的 lane 数」之和
                   / 所有向量指令上「参与统计的 lane 数」之和
```

每条 intrinsic 记 **1 条**向量指令，并带上本次操作的 `mask`：只有 mask=1 的 lane 计入「已利用」。

**根本原因：固定 9 轮会在「大量 lane 已算完」时仍继续执行内层体。**

| 对比项 | v1（动态 `while`） | v2 / v3（固定 9 轮） |
|--------|-------------------|----------------------|
| 内层次数 | 约 `max(count)` 个向量块，平均指数 0–9 时约 **4–5 轮** | 每个向量块 **固定 9 轮** |
| `count==0` 之后 | 整块退出 `while`，**不再产生** `vgt`/`vmult`/`vsub` | 仍执行满 9 轮；后几轮 `maskActive` 很稀疏 |
| 额外指令 | 每轮 `cntbits`（模拟器里按 **全 lane 活跃** 记账） | 无 `cntbits` |

因此 v2/v3 会出现大量这样的指令：循环还在跑，但一条向量里只有 1–2 个 lane 的 `count>0`，`vmult`/`vsub` 的利用率很低，把全局平均拉低到约 **66.9%**。

v1 则相反：

1. **提前结束**：指数小的 lane 先「毕业」，整块向量尽早跳出内层，少执行很多「稀疏 mask」轮次。
2. **仍执行轮次里活跃比例更高**：内层执行时，往往还有更多 lane 同时在乘。
3. **`cntbits` 的记账方式**：库实现里 `cntbits` 按 `VECTOR_WIDTH` 个 lane 全记为活跃，会抬高利用率（见 `CS149intrin.cpp` 中 `addLog("cntbits", _cs149_init_ones(), VECTOR_WIDTH)`）。

v3 与 v2 利用率相同，因为只改了 `count` 初始化（少一条 `vset`），**内层 9 轮乘方逻辑不变**；总指令数相同，利用率自然一致。

**为何 v2/v3 总指令更少，利用率却更低？**

这是两个不同指标：

- **Total Vector Instructions**：v2/v3 去掉每轮 `cntbits`，且用固定循环便于编译/模拟，总条数更少（本机 95003 vs 97075）。
- **Vector Utilization**：衡量「每条指令里 lane 有多满」，不是「指令条数」。v2/v3 用 **更多条但常稀疏** 的内层指令，换掉了 v1 **较少条但常较满** 的内层 + 全额计账的 `cntbits`。

**一句话**：v1 是「早停、少做稀疏轮」；v2/v3 是「少几条控制指令、但把乘方循环跑满」——作业打分看前者（总指令数），利用率数字体现后者（lane 是否空转）。

### 3.2 向量化常用经验

1. **先对齐作业指标**  
   prog2 性能看 **Total Vector Instructions**，不要单独追求 Utilization 百分比。利用率高但指令多，未必更快。

2. **分歧用 mask，不要过早标量化**  
   per-lane 不同的 `exponent` / `if` 应走 masked 向量指令；整段退回标量 `for` 通常指令数爆炸。

3. **有界循环可考虑固定上界**  
   本题的 `EXP_MAX` 很小，固定 `EXP_MAX-1` 轮可去掉 `cntbits`，总指令更少；代价是指数小时 lane 空转、利用率下降——在总分上仍可能更优。

4. **动态退出 vs 固定轮次**  
   - 数据相关迭代次数差异大 → 动态 `while` + `cntbits` 可能利用率更高。  
   - 迭代上界小且已知 → 固定轮次常更省总指令。  
   用 `./myexp -s 10000` 对比两版统计，不要只靠直觉。

5. **尾部：`N % VECTOR_WIDTH != 0`**  
   必须用 `_cs149_init_ones(rem)`，否则多余 lane 参与 load/store 会破坏正确性（`./myexp -s 3` 必测）。

6. **mask 语义**  
   mask=0 的 lane **不写入**结果；比较、运算要在正确的 `maskAll` / `maskActive` 下使用，避免「以为算了其实没写」。

7. **归约不要连续裸 `hadd`**  
   `arraySum` 需 `hadd` 与 `interleave` 配合，否则相邻 lane 加成重复值，结果错误。

8. **用 `-l` 看 lane 占用**  
   `./myexp -l` 打印每条向量指令的 `*`/`_` 图案，稀疏轮次一目了然，便于判断是算法分歧还是 mask 写错。

9. **利用率随 `VECTOR_WIDTH` 变宽**  
   对强分歧内核，加宽向量不一定提高利用率：同一向量内指数更「杂」，固定轮次下空 lane 更多；本报告第 6 节在 v3 下利用率随 W 几乎不变，是「稀疏比例与 W 同比例缩放」的体现，不等于「加宽无用」——总指令数仍随 W 近似下降。

10. **实现分层、便于实验**  
    像本仓库 v1/v2/v3 用宏切换，先保证正确，再在同一套验证下比统计数字，避免一次改太多无法归因。

---

## 4. `arraySum` 加分（两版）

```cpp
#ifndef ARRAY_SUM_VERSION
#define ARRAY_SUM_VERSION 2
#endif
```

| 版本 | 做法 | 复杂度 |
|------|------|--------|
| v1 | 向量分块累加 + **标量**累加各 lane | `O(N/W + W)` |
| **v2** | 向量累加 + **`hadd` + `interleave` 树形归约** | `O(N/W + log W)` |

注意：连续 `hadd` 会把相邻 lane 加成相同值，**不能**连做 `log W` 次 `hadd`；需在中间用 `interleave` 把部分和挪到同一侧，再 `hadd`（见 `arraySumVector_v2`）。

---

## 5. 测试方法

```bash
cd prog2_vecintrin
make

# 正确性：尾部 N % W != 0
./myexp -s 3

# 性能与利用率
./myexp -s 10000

# 指令级调试
./myexp -s 16 -l
```

修改 `CS149intrin.h` 中 `#define VECTOR_WIDTH` 后需 `make clean && make`。

---

## 6. 利用率随 `VECTOR_WIDTH` 的变化（v3，`N=10000`）

| VECTOR_WIDTH | Total Vector Instructions | Vector Utilization |
|--------------|---------------------------|--------------------|
| 2 | 190003 | 66.9% |
| 4 | 95003 | 66.9% |
| 8 | 47503 | 66.9% |
| 16 | 23753 | 66.9% |

**现象**：利用率**基本不变**；总指令数随 `W` 增大近似按 `1/W` 下降（外层次数 ∝ `N/W`）。

**原因（固定迭代实现）**

- 内层固定 `EXP_MAX-1` 轮，每轮 `vgt`/`vmult`/`vsub` 的活跃 lane 比例由随机指数分布决定，与 `W` 成比例缩放
- `N=10000` 可被 2/4/8/16 整除，**无尾部** partial-vector 影响
- 若用 v1 动态 `while`，不同 `W` 下利用率可能略有波动，但渐近仍受「lane 分歧」主导

---

## 7. 与 `absVector` 示例的关系

`absVector` 演示了 load → 比较生成 mask → 双分支 masked 写 → store。

作业特别强调：当 `N % VECTOR_WIDTH != 0` 时，示例**可能错误**（未对尾部用 `_cs149_init_ones(rem)`）。`clampedExp` 实现中每轮：

```cpp
maskAll = _cs149_init_ones(std::min(VECTOR_WIDTH, N - i));
```

---

## 8. 文件说明

| 文件 | 作用 |
|------|------|
| `main.cpp` | `clampedExpVector_v1/v2/v3`、`arraySumVector_v1/v2` 及宏分发 |
| `CS149intrin.h` | `VECTOR_WIDTH`、向量 API |
| `REPORT.md` | 本文档 |

---

## 9. 小结

1. **mask + 外层分块** 是向量化控制流的核心，不是简单把 `i++` 改成 `i += W`。
2. **动态 while vs 固定迭代** 是指令数与利用率的权衡（详见 §3.1）；本作业性能指标偏向 **v3**，利用率低不等于实现更差。
3. **尾部** 与 **`exp==0`** 是常见 bug 来源，需单独验证 `./myexp -s 3`。
4. **归约** 需 `hadd`/`interleave` 配合，不能盲目连续 `hadd`。
