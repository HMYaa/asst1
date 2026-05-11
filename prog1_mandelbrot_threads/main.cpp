#include <stdio.h>
#include <algorithm>
#include <cstring>
#include <getopt.h>

#include "CycleTimer.h"

extern void mandelbrotSerial(
    float x0, float y0, float x1, float y1,
    int width, int height,
    int startRow, int numRows,
    int maxIterations,
    int output[]);

extern void mandelbrotThread(
    int numThreads,
    float x0, float y0, float x1, float y1,
    int width, int height,
    int maxIterations,
    int output[]);

extern void writePPMImage(
    int* data,
    int width, int height,
    const char *filename,
    int maxIterations);

// scaleAndShift: 对复平面视口坐标做缩放和平移
// 用于切换 view2（放大 Mandelbrot 集边界某处细节）
// scale 越小，视口越窄，图像越"放大"
void
scaleAndShift(float& x0, float& x1, float& y0, float& y1,
              float scale,
              float shiftX, float shiftY)
{

    x0 *= scale;
    x1 *= scale;
    y0 *= scale;
    y1 *= scale;
    x0 += shiftX;
    x1 += shiftX;
    y0 += shiftY;
    y1 += shiftY;

}

void usage(const char* progname) {
    printf("Usage: %s [options]\n", progname);
    printf("Program Options:\n");
    printf("  -t  --threads <N>  Use N threads\n");
    printf("  -v  --view <INT>   Use specified view settings\n");
    printf("  -?  --help         This message\n");
}

// verifyResult: 逐像素对比串行结果（gold）与多线程结果
// 用于验证多线程实现的正确性——结果必须与串行完全一致
// 发现不一致时打印出错位置及期望/实际值，便于调试
bool verifyResult (int *gold, int *result, int width, int height) {

    int i, j;

    for (i = 0; i < height; i++) {
        for (j = 0; j < width; j++) {
            if (gold[i * width + j] != result[i * width + j]) {
                printf ("Mismatch : [%d][%d], Expected : %d, Actual : %d\n",
                            i, j, gold[i * width + j], result[i * width + j]);
                return 0;
            }
        }
    }

    return 1;
}

int main(int argc, char** argv) {

    // 图像分辨率：1600x1200，共 1,920,000 个像素，每个像素独立计算
    const unsigned int width = 1600;
    const unsigned int height = 1200;
    // maxIterations：判定一个复数点是否属于 Mandelbrot 集的最大迭代次数
    // 值越大，图像越精细，计算量越大；集合内部的点会迭代满 256 次
    const int maxIterations = 256;
    int numThreads = 2;  // 默认 2 线程，可通过 -t 参数覆盖

    // 复平面视口：x ∈ [-2, 1]，y ∈ [-1, 1]
    // 每个像素 (i,j) 对应复数 c = (x0 + i*dx) + (y0 + j*dy)*i
    float x0 = -2;
    float x1 = 1;
    float y0 = -1;
    float y1 = 1;

    // parse commandline options ////////////////////////////////////////////
    int opt;
    static struct option long_options[] = {
        {"threads", 1, 0, 't'},
        {"view", 1, 0, 'v'},
        {"help", 0, 0, '?'},
        {0 ,0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "t:v:?", long_options, NULL)) != EOF) {

        switch (opt) {
        case 't':
        {
            numThreads = atoi(optarg);
            break;
        }
        case 'v':
        {
            int viewIndex = atoi(optarg);
            // view2：放大 Mandelbrot 集边界细节区域
            // scale=0.015 将视口缩小约 67 倍，shift 移到边界附近
            // 边界区域计算量更均匀（更多点需要接近 maxIterations 次迭代），
            // 是测试负载均衡效果的另一种场景
            if (viewIndex == 2) {
                float scaleValue = .015f;
                float shiftX = -.986f;
                float shiftY = .30f;
                scaleAndShift(x0, x1, y0, y1, scaleValue, shiftX, shiftY);
            } else if (viewIndex > 1) {
                fprintf(stderr, "Invalid view index\n");
                return 1;
            }
            break;
        }
        case '?':
        default:
            usage(argv[0]);
            return 1;
        }
    }
    // end parsing of commandline options


    int* output_serial = new int[width*height];
    int* output_thread = new int[width*height];

    //
    // Run the serial implementation.  Run the code 5 times and
    // take the minimum to get a good estimate.
    //
    // [为什么取最小值而非平均值？]
    // 微基准测试（microbenchmark）标准做法：
    //   - 最小值代表"最理想状态"——CPU 缓存热、OS 调度无干扰时的性能上限
    //   - 平均值会被偶发的 OS 调度抖动、TLB miss、内存缺页等噪声拉高
    //   - 目标是测量算法本身的性能，而非系统噪声

    double minSerial = 1e30;
    for (int i = 0; i < 5; ++i) {
       memset(output_serial, 0, width * height * sizeof(int));
        double startTime = CycleTimer::currentSeconds();
        mandelbrotSerial(x0, y0, x1, y1, width, height, 0, height, maxIterations, output_serial);
        double endTime = CycleTimer::currentSeconds();
        minSerial = std::min(minSerial, endTime - startTime);
    }

    printf("[mandelbrot serial]:\t\t[%.3f] ms\n", minSerial * 1000);
    writePPMImage(output_serial, width, height, "mandelbrot-serial.ppm", maxIterations);

    //
    // Run the threaded version
    //

    double minThread = 1e30;
    for (int i = 0; i < 5; ++i) {
      memset(output_thread, 0, width * height * sizeof(int));
        double startTime = CycleTimer::currentSeconds();
        mandelbrotThread(numThreads, x0, y0, x1, y1, width, height, maxIterations, output_thread);
        double endTime = CycleTimer::currentSeconds();
        minThread = std::min(minThread, endTime - startTime);
    }

    printf("[mandelbrot thread]:\t\t[%.3f] ms\n", minThread * 1000);
    writePPMImage(output_thread, width, height, "mandelbrot-thread.ppm", maxIterations);

    // 正确性验证：多线程结果必须与串行结果逐像素一致
    // 若不一致，说明线程间存在数据竞争或工作分配有 bug
    if (! verifyResult (output_serial, output_thread, width, height)) {
        printf ("Error : Output from threads does not match serial output\n");

        delete[] output_serial;
        delete[] output_thread;

        return 1;
    }

    // 加速比 = 串行耗时 / 多线程耗时
    // 理论上限（Amdahl 定律）：若程序 100% 可并行，N 线程加速比上限为 N
    // 实际受限于：线程创建开销、负载不均衡、内存带宽竞争、超线程共享资源等
    // 目标：8 线程在两个 view 下均达到约 7-8x 加速
    printf("\t\t\t\t(%.2fx speedup from %d threads)\n", minSerial/minThread, numThreads);

    delete[] output_serial;
    delete[] output_thread;

    return 0;
}
