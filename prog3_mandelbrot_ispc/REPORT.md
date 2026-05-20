# Program 3 实验报告：ISPC 与 Mandelbrot 多核 SIMD 并行

## 1. 作业目标

| 部分 | 分值 | 目标 |
|------|------|------|
| **Part 1：ISPC / SIMD** | 10 分 | 理解 `foreach`、gang、8 宽 AVX2；分析 `mandelbrot_ispc` 相对串行的加速及为何低于 8× |
| **Part 2：ISPC 任务** | 10 分 | **仅修改** `mandelbrot_ispc_withtasks()`，使 `--tasks` 版本相对 `mandelbrotSerial()` **超过 32×** |
| 加分 | 2 分 | 比较 `std::thread` 与 ISPC `launch/sync` 抽象（可选论述） |

程序在 `prog3_mandelbrot_ispc/`：`mandelbrotSerial.cpp`（串行参考）、`mandelbrot.ispc`（ISPC 实现）、`main.cpp`（计时与校验）。

与 **Program 1** 相同问题（Mandelbrot 图像），但并行层次不同：

- **Prog1**：多线程，每线程一段连续行（或行交错）→ **线程级** 多核。
- **Prog3**：`foreach` → 单核内 **SIMD**；`launch` → **任务级** 多核。二者叠加才接近「4 核 × 8 宽 ≈ 32×」。

---

## 2. 需要掌握的知识点

### 2.1 Mandelbrot 与负载特征（与 Prog1 共通）

对每个像素 \((x,y)\)，在复平面上迭代 \(z \leftarrow z^2 + c\) 直到 \(|z|^2 > 4\) 或达到 `maxIterations`。返回值 = 迭代次数（亮度 ∝ 代价）。

**关键特征**：像素/行之间**完全独立**，但**代价极不均匀**——集合边界附近迭代多，外部很快退出。这是 SIMD 与多核调度都要面对的 **control flow divergence / 负载不均**。

### 2.2 ISPC 执行模型（必读 README 中「友善教师」段）

- 从 C 调用 `export` 的 ISPC 函数 → 启动一组 **gang** 的 **program instances**，在 SIMD 单元上并行执行。
- `programCount`：gang 宽度（本作业 Makefile 为 `avx2-i32x8` → **8**）。
- `programIndex`：当前实例在 gang 中的下标。
- **`uniform`**：所有实例相同的标量（如 `width`、`x0`）。
- **非 uniform（varying）**：每个 lane 可不同（如每个像素的 `x,y`、迭代次数）。

**Part 1 的 `mandelbrot_ispc` 只用到单 gang 的 `foreach`**，编译器把迭代映射到 SIMD，**默认不会自动跨物理核**展开（与 Prog1 多线程不同）。

### 2.3 `foreach`：声明式独立迭代

```ispc
foreach (j = 0 ... height, i = 0 ... width) {
    // 每次 (j,i) 迭代彼此独立，可任意顺序、由 ISPC 映射到 SIMD
}
```

对比命令式 `for (i+=programCount)`：只描述**有哪些独立工作**，由编译器负责映射到实例，是推荐写法。

### 2.4 ISPC 任务：`launch` / `sync`

```ispc
task void mandelbrot_ispc_task(...) { ... }  // 每个 task 一个 gang

export void mandelbrot_ispc_withtasks(...) {
    launch[numTasks] mandelbrot_ispc_task(...);  // 隐式 sync：全部 task 结束后返回 C
}
```

- `taskIndex`：当前 task 编号（0 … numTasks−1）。
- 每个 task 内仍可 `foreach`，即 **task 间多核 + task 内 SIMD**。
- `launch[N]` 的 **N 须为编译期常数**。

### 2.5 理论加速上界（myth：4 核，每核 8 宽 AVX2）

| 层次 | 理想倍数 | 说明 |
|------|----------|------|
| 仅 SIMD（`mandelbrot_ispc`） | ≤ **8×** | 8 个 float lane 同时算不同像素 |
| SIMD + 4 核（`--tasks` 调优后） | ≤ **32×** | 4 个 task 同时跑在不同核，各核内 8 宽 |
| 实际 | 低于上界 | 见下文「第一性原理」 |

---

## 3. 第一性原理：为何慢、优化从哪出发

### 3.1 正确性前提

每个 `output[index]` 只依赖对应 \((x,y)\)，无共享写冲突。并行化只需保证：**每个像素恰好算一次**，与串行 `mandel()` 语义一致。Starter 的 `foreach` 与 `mandel()` 已满足，**无需改算法**。

### 3.2 SIMD 有效加速的瓶颈（Part 1）

在 `mandel()` 内层：

```ispc
for (i = 0; i < count; ++i) {
    if (z_re * z_re + z_im * z_im > 4.f) break;  // 每 lane 可能不同步退出
    ...
}
```

- 8 个 lane 各算一个像素，**退出迭代次数不同** → 存在 **masked / 空转**，类似 Prog2 的 mask 利用率问题。
- 视图 1：大面积「很快退出」或「算满 256 次」的区域，lane 间较一致 → SIMD 利用率较高。
- 视图 2（`--view 2`）：放大边界，**更多中等迭代次数** 混在一起 → divergence 更严重，ISPC 加速比往往**更低**。

因此：**理论最大 8×，实测约 3.5–4.5×（本机）属正常**，不是实现错误。

### 3.3 多核有效加速的瓶颈（Part 2）

Starter 代码：

```ispc
uniform int rowsPerTask = height / 2;
launch[2] ...
```

- 只有 **2 个 task** → 同一时刻最多约 **2 个核** 在跑 ISPC task（其余核空闲）。
- 再叠加 **行与行之间代价差异**（与 Prog1 块划分类似）：快 task 先结束，慢 task 拖尾 → **straggler**。
- 故 `--tasks` 相对串行仅约 **8×**（本机），远低于 32×。

**优化出发点（与 Prog1 行交错同一逻辑）**：

1. **增加 task 数** → 占满多核（≥ 物理核数，通常取 2×～数倍核数）。
2. **缩小每个 task 的行范围** → 把「重行」「轻行」打散到不同 task，降低 straggler。
3. 在 `launch` 开销可接受范围内取平衡；`height=800` 时 **32 个 task**（每 task 约 25 行）在 myth 上即可 **>32×**。

**不做的「表面优化」**：不改 `mandel()` 数学、不强行向量化内层 while（作业不要求且易破坏与串行一致）；Part 2 **只改** `mandelbrot_ispc_withtasks()`。

---

## 4. 实现：逐步增加 task（Part 2）

仅修改 `mandelbrot_ispc_withtasks()`：

| 阶段 | `launch[N]` | `rowsPerTask` | 相对串行加速（本机 Xeon，view 1） | 说明 |
|------|-------------|---------------|-----------------------------------|------|
| 初始 | 2 | `height/2` | ~8× | 2 核级并行，负载不均明显 |
| 步进 1 | 8 | `⌈h/8⌉` | ~18× | 更多核参与 |
| 步进 2 | 16 | `⌈h/16⌉` | ~33× | 达到作业 **32×** 门槛 |
| **提交** | **32** | **`⌈h/32⌉`** | **~64×** | 进一步打散 straggler，留余量 |

最终代码：

```ispc
const uniform int numTasks = 32;
uniform int rowsPerTask = (height + numTasks - 1) / numTasks;
launch[numTasks] mandelbrot_ispc_task(...);
```

**如何确定 task 数量？**

1. **下界**：不少于 **物理核数**（myth 为 4），否则核闲置。
2. **上界**：受 `launch` 调度开销限制；对 Mandelbrot 这种**重负载、高不均** 问题，可远大于核数（如 16–32，甚至 `height` 行级 task）。
3. **经验**：从 `2 × 核数` 起扫，至加速比平台期；本机 `numTasks=16` 已 ≈33×，`32` 更稳。
4. **与 Prog1 对照**：`numTasks=32`、每 task 多行 ≈ **静态粗粒度行交错**；`numTasks=height` 即每行一 task，负载最匀但调度最重。

---

## 5. Part 1 实验记录与分析（本机）

图像：1200×800，`maxIterations=256`。ISPC：`--target=avx2-i32x8`。

### 5.1 视图 1（默认）

| 实现 | 时间 (ms) | 相对串行加速 |
|------|-----------|--------------|
| serial | ~322 | 1× |
| `mandelbrot_ispc` | ~75 | **~4.3×** |
| `mandelbrot_ispc --tasks`（32 tasks） | ~5 | **~64×** |

**理论 8× vs 实测 ~4.3×**：主因是 `mandel()` 内 **per-lane 早退** 导致 SIMD lane 空转；次要因素含内存带宽、函数调用开销等。

### 5.2 视图 2（`--view 2`）

| 实现 | 相对串行加速 |
|------|--------------|
| ISPC（无 tasks） | ~3.6× |
| ISPC + tasks | ~6.1×（2 tasks 时） / 更高（32 tasks 时） |

视图 2 边界更复杂，**divergence 加重** → 无 tasks 的 SIMD 加速比 **低于视图 1**，验证「边界区域对 SIMD 更难」的假设。

### 5.3 为何 `foreach` 与 `launch` 两套机制？

两层并行**正交**：`foreach` 管「核内 lane」，`launch` 管「核间 task」。下图以 4 核、8 宽 SIMD、图像按行划分为例。

**仅 `foreach`（`mandelbrot_ispc`）—— 单 gang，占 1 核**

```
  图像行 j=0 ─────────────────────────────►
              ┌── foreach 一次推进 8 个 (j,i) ──┐
  Core0       │ L0 L1 L2 L3 L4 L5 L6 L7 │ SIMD   ← 8 lane 同时算 8 像素
  (唯一活跃)  └──────────────────────────┘
  Core1       [ 空闲 ]
  Core2       [ 空闲 ]
  Core3       [ 空闲 ]
```

**`launch` + `foreach`（`--tasks`）—— 多 gang，各占 1 核**

```
  图像行 j=0 ─────────────────────────────►
              ┌─ task0 负责的行块 ─┐
  Core0       │ foreach → 8 lane │  rows 0..24
              └──────────────────┘
  Core1       │ foreach → 8 lane │  rows 25..49
  Core2       │ foreach → 8 lane │  rows 50..74
  Core3       │ foreach → 8 lane │  rows 75..99
              ...
  (更多 task 轮转调度到各核，此处简化为 4 task / 4 核)
```

**对比（职责分离）**

```
  并行维度          foreach                    launch
  ─────────────────────────────────────────────────────────
  粒度              1 次迭代 ≈ 1 像素/元组      1 个 task ≈ 多行图像块
  映射目标          同一核上的 SIMD lane        不同核上的 gang
  谁负责调度        ISPC 编译器                 ISPC 运行时 (+ 你设 N)
  本作业若不写      有 SIMD，无多核              无 launch → 核闲置
```

**为何不自动把 `foreach` 拆到所有核？**

```
  理想「全自动」：编译器把全部 (j,i) 迭代摊到 4 核 × 每核 8 lane
                  → 需全局划分 + 每核生成最优 SIMD + 负载均衡

  ISPC 实际做法：foreach → 单核 SIMD（确定、简单）
                  launch  → 你显式切行块，核间并行（本作业调 N）
```

- `foreach`：声明 **lane 级** 独立迭代 → 编译器映射到 **单核 SIMD**。
- `launch`：声明 **gang 级** 独立任务 → 运行时映射到 **多核**。
- 两层分开后，程序员直接控制 **核间** 并行度（`launch[N]`），不必依赖编译器做跨核 + SIMD 联合调度。

---

## 6. 测试方法

```bash
cd prog3_mandelbrot_ispc
make
./mandelbrot_ispc              # serial + ISPC，校验 PPM
./mandelbrot_ispc -t           # 含 multicore ISPC tasks
./mandelbrot_ispc -v 2         # 视图 2
./mandelbrot_ispc -v 2 -t
```

输出对比 `verifyResult`；图像为 `mandelbrot-*.ppm`。

在 **Stanford myth**（4 核 i7，2 超线程）上应用相同 `numTasks=32` 应满足 **>32×**；本地核数更多时加速比会更高，属正常现象。

---

## 7. 与 Program 1 / 2 的联系

| 概念 | Prog1 | Prog2 | Prog3 |
|------|-------|-------|-------|
| 并行单元 | `std::thread` | 模拟 SIMD lane | ISPC gang + `launch` task |
| 负载不均 | 行交错 | mask / 固定轮次 | 行级 task 切分 |
| 控制流分歧 | 线程间独立 | `mask` + `cntbits` | `mandel()` 内早退 |
| 关键 API | `join` | `_cs149_*` | `foreach`, `launch` |

---

## 8. 加分题要点（线程 vs ISPC task，简述）

| | `std::thread` | ISPC `launch` |
|---|---------------|---------------|
| 粒度 | OS 线程，较重 | 轻量 task，由 ISPC 运行时调度 |
| 数量 | 上万线程 → 调度崩溃、内存爆炸 | 上万 task 相对可行（仍有限制） |
| 与 SIMD | 需自己写 intrinsics / 编译器自动向量化 | gang 内 SIMD 与语法绑定 |
| 同步 | `join`, mutex 等 | `launch` 隐式 `sync` |

---

## 9. 文件说明

| 文件 | 作用 |
|------|------|
| `mandelbrotSerial.cpp` | 串行参考 |
| `mandelbrot.ispc` | `mandelbrot_ispc`、`mandelbrot_ispc_withtasks`（**Part 2 修改处**） |
| `main.cpp` | 计时、`-t` / `-v`、正确性检查 |
| `Makefile` | `ispc --target=avx2-i32x8` |
| `REPORT.md` | 本报告 |

---

## 附录：常见疑问（抽象表述）

以下把实验里容易混淆的点写成**与具体机器、具体 N 无关**的通式，便于对照任意 SPMD + task 程序（不限 Mandelbrot）。

### A.1 `foreach` 会不会自动用到所有核？

**不会（就「跨物理核」而言）。**

| 概念 | 抽象含义 |
|------|----------|
| 一次 `export` 调用 + 仅含 `foreach` | 通常对应 **一个 gang**，在 **单核** 上用 **W 个 lane**（SIMD 宽度）并行 |
| `foreach` 的「自动」 | 在 **核内** 把独立迭代映射到 lane，**不是** 在 **核间** 分核 |
| 要多核 | 需 **`launch`**、多次调用、或外层线程等 **另一层** 并行机制 |

```
  仅 foreach：     1 gang → 1 核 × W lane
  launch+foreach： T 个 task → 最多约 P 核同时各跑 1 gang（每 gang 仍 W lane）
```

**通式上界（相对单线程标量参考实现）**：仅 SIMD 时加速比 ≲ **W**，且常因控制流分歧再打折。

---

### A.2 `launch[N]` 且 **N > 物理核数 P** 时，如何调度？程序员感知什么？

**模型：任务队列 + 固定规模的 worker，不是「每个 task 绑一个 OS 线程」。**

```
  launch[N]  →  N 份独立 task 入队（每份带 taskIndex）
                ↓
  ≈ P 个 worker（+ 有时调用线程在 sync 时帮忙）
                ↓
  同一时刻：min(N, P_可用) 份 task 在执行
  其余：在队列中等待「某个 worker 空闲」
                ↓
  某 task 整段跑完 → worker 再取队首/队尾下一 task
                ↓
  N 份全部完成 → 隐式 sync 返回
```

| 程序员 | 运行时 |
|--------|--------|
| **要管**：`N`、用 `taskIndex` 划分互不重叠的数据 | **管**：哪个 worker 何时执行哪一份 |
| **要管**：`launch` 返回前所有 task 已完成（同步语义） | **不管**：一般不暴露「task k 在第几号核」 |
| **不必管**：绑核、时间片、抢占 | **不做**：在单个 task **内部** 因时间片抢占（见 A.3） |

**N > P 的意义**：不是同时占满 N 个核，而是 **完成一份再领一份**——在 **单份 task 耗时起伏很大** 时，用更多份数把重/轻活打散，减轻 **straggler**（与多线程里「任务数 > 线程数」同构）。

---

### A.3 加速来自「阻塞时的调度切换」吗？

**不是。** ISPC task 在本作业使用的运行时里，典型语义是：

- **粒度**：以 **整 task** 为单位调度；
- **执行**：worker 取到一个 task 后 **连续跑完** 再取下一个；
- **不像** OS 在循环中间因 I/O 或时间片 **抢占** 线程。

加速来源应写成：

```text
加速 ≈（有效并行份数）×（每份内的 SIMD 或标量效率）×（负载均衡）−（调度/同步开销）
```

**不是**「切换越频繁越快」，而是 **同一时刻有更多核在各自算不同的独立份**。

---

### A.4 为何实测加速比会 **高于或低于** 「P 核 × W 宽」？

作业或讲义里的 **P×W** 是针对 **某一参考机**（例如 4 核、8 宽 → 32×）相对 **单线程标量** 的**理想上界**，不是任意环境下的保证值。

| 现象 | 抽象原因 |
|------|----------|
| **低于 P×W** | 仅用了单核 SIMD；或 SIMD 分歧使有效宽度 < W；或 N≪P 核闲置；或 straggler；或内存带宽饱和 |
| **高于参考课的 P×W** | 实测机 **P_实测 > P_参考**（更多核/线程参与）；与「作业目标 32×」比较时，**硬件更强** 即可出现更大倍数 |
| **N 很大仍快** | 主要因为 **N 与 P 的关系** 和 **每份工作量**，不是因为在 task 内部做了抢占切换 |

**合并估算（心智模型）**：

```text
T_serial   ：单线程标量算完全部工作
T_parallel ：约 (工作总量 / min(N, P_eff)) / eff_SIMD  + T_overhead

加速比 ≈ T_serial / T_parallel
```

- **P_eff**：实际参与干活的核数（≤ 机器核数，≤ N）；
- **eff_SIMD**：核内因分歧、内存等，有效 lane 利用率（≤ W）；
- **T_overhead**：入队、`sync`、锁等（N 过大时上升）。

因此：**「launch[32] 得到 64×」** 应读成「相对**单线程标量**，有效并行度 × SIMD 效率的乘积」，而不是「32 个 task 产生了 2 倍的调度魔法」；在 **核数多于参考机** 的机器上，**超过 32× 并不矛盾**。

---

### A.5 三层并行别混在一行里加

```text
  层 1（参考实现） ：1 线程 × 1 路标量
  层 2（foreach）   ：1 核 × W lane（SPMD / SIMD）
  层 3（launch）    ：最多约 P 核 × 各跑 1 gang（task 级）

  若只写层 2：加速比 ~ O(W) 且打折
  若写层 2+3：加速比 ~ O(min(N,P) × W) 且打折
```

调试时先确认 **层 2 是否正确**，再加 **层 3**；否则会把「核没跑满」误判成「SIMD 写得不对」。

---

## 10. 小结

1. **Part 1** 理解 ISPC gang 与 `foreach`；Mandelbrot 内层早退使 **SIMD 加速远低于 8×** 是算法特性，非实现缺陷。
2. **Part 2** 瓶颈是 **task 太少（launch[2]）** 与 **行间负载不均**；通过 **`numTasks=32` + 向上取整的 `rowsPerTask`** 同时提高核利用率与负载均衡，达到 **>32×**（在参考多核机上）；更高倍数通常表示 **实测机并行度大于参考机**，见附录 A.4。
3. 优化与 Prog1 **行交错** 同构：在**不改动像素语义**的前提下，调整**独立任务粒度**，从第一性原理解决 straggler，而非改 `mandel()` 公式。
4. **调度语义**：`launch` 的收益来自 **多份独立 task 并行 + 核内 SIMD**，不是 task 内部的抢占切换；详见附录 A.2–A.3。
