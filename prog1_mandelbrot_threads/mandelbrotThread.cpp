#include <stdio.h>
#include <cstdlib>  // 引入标准库，exit() 在 mandelbrotThread() 中用于线程数超限时终止程序
#include <thread>

#include "CycleTimer.h"

// WorkerArgs: 线程参数结构体
// 每个线程持有一份该结构体，描述"这个线程负责渲染哪块区域"
// 
// 设计要点：
//   - x0/y0/x1/y1、width/height、maxIterations 对所有线程相同（只读，无竞争）
//   - output 是所有线程共享的输出数组指针，但每个线程写入不同的行（index 不重叠），
//     因此无需加锁——这是 embarrassingly parallel 并行模式的典型体现
//   - threadId + numThreads 是工作分配的唯一依据（Cyclic 分配策略）
typedef struct {
    float x0, x1;
    float y0, y1;
    unsigned int width;
    unsigned int height;
    int maxIterations;
    int* output;
    int threadId;
    int numThreads;
} WorkerArgs;


extern void mandelbrotSerial(
    float x0, float y0, float x1, float y1,
    int width, int height,
    int startRow, int numRows,
    int maxIterations,
    int output[]);


//
// workerThreadStart -- Thread entrypoint.
//
// [实现思路] Cyclic（交错）行分配策略：
//   线程 i 负责处理行号为 i, i+N, i+2N, i+3N, ... 的所有行（N = numThreads）
//
//   例如 4 线程、图像高度 12 行：
//     线程 0：行 0, 4, 8
//     线程 1：行 1, 5, 9
//     线程 2：行 2, 6, 10
//     线程 3：行 3, 7, 11
//
// [为什么不用 Block 分配？]
//   Block 分配让线程 0 处理行 0-299，线程 1 处理行 300-599……
//   但 Mandelbrot 图像计算量分布不均匀：
//     - 集合内部（图像中央）的像素需要迭代满 maxIterations=256 次才能判定
//     - 集合外部（图像边缘）的像素迭代很少次就逃逸（提前 break）
//   Block 分配会让负责中央区域的线程远比边缘线程慢，造成负载不均衡（Load Imbalance）
//   其他线程空等，浪费 CPU 资源，整体加速比大幅下降。
//
//   Cyclic 分配让每个线程都"采样"到图像各个位置的行，
//   统计上均摊了计算量，各线程完成时间接近，负载均衡效果好。
//
// [每次只处理 1 行（numRows=1）]
//   每次调用 mandelbrotSerial() 传入 numRows=1，粒度最细，
//   配合 Cyclic 步进，实现了对任意线程数都通用的工作分配，
//   无需根据线程数硬编码区间边界。
//
void workerThreadStart(WorkerArgs * const args) {

    // TODO FOR CS149 STUDENTS: Implement the body of the worker
    // thread here. Each thread should make a call to mandelbrotSerial()
    // to compute a part of the output image.  For example, in a
    // program that uses two threads, thread 0 could compute the top
    // half of the image and thread 1 could compute the bottom half.

    // Cyclic 分配：从 threadId 行开始，每次跳过 numThreads 行
    // for (unsigned int row = args->threadId; row < args->height; row += args->numThreads) {
    //     mandelbrotSerial(args->x0, args->y0, args->x1, args->y1,
    //                      args->width, args->height,
    //                      row, 1,          // startRow=row, numRows=1（每次处理一行）
    //                      args->maxIterations,
    //                      args->output);
    // }

    // Block 按行划分：每线程约 height/numThreads 行，余数行分给前 rem 个线程
    const unsigned int height = args->height;
    const int tid = args->threadId;
    const int n = args->numThreads;

    const unsigned int baseRows = height / (unsigned int)n;
    const unsigned int rem = height % (unsigned int)n;

    const unsigned int startRow =
        (unsigned int)tid * baseRows + (unsigned int)((tid < (int)rem) ? tid : rem);
    const unsigned int numRows = baseRows + (unsigned int)((tid < (int)rem) ? 1 : 0);

    if (numRows > 0) {
        mandelbrotSerial(args->x0, args->y0, args->x1, args->y1,
                         (int)args->width, (int)args->height,
                         (int)startRow, (int)numRows,
                         args->maxIterations,
                         args->output);
    }
}

//
// MandelbrotThread --
//
// Multi-threaded implementation of mandelbrot set image generation.
// Threads of execution are created by spawning std::threads.
void mandelbrotThread(
    int numThreads,
    float x0, float y0, float x1, float y1,
    int width, int height,
    int maxIterations, int output[])
{
    static constexpr int MAX_THREADS = 32;

    if (numThreads > MAX_THREADS)
    {
        fprintf(stderr, "Error: Max allowed threads is %d\n", MAX_THREADS);
        exit(1);
    }

    // Creates thread objects that do not yet represent a thread.
    std::thread workers[MAX_THREADS];
    WorkerArgs args[MAX_THREADS];

    // 为每个线程初始化参数
    // 注意：所有线程共享相同的只读参数（坐标范围、图像尺寸等）
    // 唯一区分线程的是 threadId，它决定了 Cyclic 分配中该线程负责哪些行
    for (int i=0; i<numThreads; i++) {

        // TODO FOR CS149 STUDENTS: You may or may not wish to modify
        // the per-thread arguments here.  The code below copies the
        // same arguments for each thread
        args[i].x0 = x0;
        args[i].y0 = y0;
        args[i].x1 = x1;
        args[i].y1 = y1;
        args[i].width = width;
        args[i].height = height;
        args[i].maxIterations = maxIterations;
        args[i].numThreads = numThreads;
        args[i].output = output;

        args[i].threadId = i;
    }

    // 启动 numThreads-1 个新线程（从 i=1 开始），主线程自身作为 worker 0 参与计算
    // 这样做的好处：节省一次线程创建/销毁开销，主线程不空等
    for (int i=1; i<numThreads; i++) {
        workers[i] = std::thread(workerThreadStart, &args[i]);
    }

    // 主线程执行 threadId=0 的工作（行 0, N, 2N, ...）
    workerThreadStart(&args[0]);

    // 等待所有 worker 线程完成（barrier）
    // join() 保证：主线程在所有子线程写完 output[] 之后才继续
    for (int i=1; i<numThreads; i++) {
        workers[i].join();
    }
}
