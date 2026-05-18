#ifndef MANDELBROT_THREAD_H_
#define MANDELBROT_THREAD_H_

enum PartitionStrategy {
    PARTITION_BLOCK = 0,       // contiguous row blocks (steps 1–2)
    PARTITION_INTERLEAVED = 1, // cyclic rows (~7–8x target)
};

void mandelbrotThread(
    int numThreads,
    float x0, float y0, float x1, float y1,
    int width, int height,
    int maxIterations, int output[]);

void mandelbrotThreadWithStrategy(
    int numThreads,
    float x0, float y0, float x1, float y1,
    int width, int height,
    int maxIterations, int output[],
    PartitionStrategy strategy,
    bool profileThreads);

#endif  // MANDELBROT_THREAD_H_
