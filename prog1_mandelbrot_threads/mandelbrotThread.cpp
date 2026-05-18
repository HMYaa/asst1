#include "mandelbrotThread.h"

#include <algorithm>
#include <cstdlib>
#include <stdio.h>
#include <thread>

#include "CycleTimer.h"

struct WorkerArgs {
    float x0, x1;
    float y0, y1;
    unsigned int width;
    unsigned int height;
    int maxIterations;
    int* output;
    int threadId;
    int numThreads;
    int startRow;  // block partition only
    int numRows;
};

extern void mandelbrotSerial(
    float x0, float y0, float x1, float y1,
    int width, int height,
    int startRow, int numRows,
    int maxIterations,
    int output[]);

static void mandelbrotRowRange(const WorkerArgs* args, int startRow, int numRows) {
    if (numRows <= 0) {
        return;
    }
    mandelbrotSerial(
        args->x0, args->y0, args->x1, args->y1,
        args->width, args->height,
        startRow, numRows,
        args->maxIterations, args->output);
}

static void computeBlockRange(
    int threadId, int numThreads, int height,
    int* startRow, int* numRows) {
    const int rowsPerThread = (height + numThreads - 1) / numThreads;
    *startRow = threadId * rowsPerThread;
    *numRows = std::min(rowsPerThread, height - *startRow);
}

static void workerThreadStart_Block(WorkerArgs* const args) {
    mandelbrotRowRange(args, args->startRow, args->numRows);
}

static void workerThreadStart_Interleaved(WorkerArgs* const args) {
    for (int j = args->threadId; j < static_cast<int>(args->height);
         j += args->numThreads) {
        mandelbrotRowRange(args, j, 1);
    }
}

using WorkerFn = void (*)(WorkerArgs* const);

static void runWorker(
    WorkerArgs* const args,
    WorkerFn workerFn,
    bool profileThreads,
    bool isBlock) {
    const double t0 = profileThreads ? CycleTimer::currentSeconds() : 0.0;
    workerFn(args);
    if (!profileThreads) {
        return;
    }
    const double ms = (CycleTimer::currentSeconds() - t0) * 1000.0;
    if (isBlock) {
        printf("[block thread %d] %.3f ms  rows [%d, %d)\n",
               args->threadId, ms, args->startRow, args->startRow + args->numRows);
    } else {
        printf("[interleaved thread %d] %.3f ms\n", args->threadId, ms);
    }
}

static void mandelbrotThreadImpl(
    int numThreads,
    float x0, float y0, float x1, float y1,
    int width, int height,
    int maxIterations, int output[],
    WorkerFn workerFn,
    bool useBlockRanges,
    bool profileThreads) {
    static constexpr int MAX_THREADS = 32;

    if (numThreads > MAX_THREADS) {
        fprintf(stderr, "Error: Max allowed threads is %d\n", MAX_THREADS);
        exit(1);
    }

    std::thread workers[MAX_THREADS];
    WorkerArgs args[MAX_THREADS];

    for (int i = 0; i < numThreads; i++) {
        args[i].x0 = x0;
        args[i].y0 = y0;
        args[i].x1 = x1;
        args[i].y1 = y1;
        args[i].width = static_cast<unsigned int>(width);
        args[i].height = static_cast<unsigned int>(height);
        args[i].maxIterations = maxIterations;
        args[i].numThreads = numThreads;
        args[i].output = output;
        args[i].threadId = i;

        if (useBlockRanges) {
            computeBlockRange(i, numThreads, height, &args[i].startRow, &args[i].numRows);
        } else {
            args[i].startRow = 0;
            args[i].numRows = 0;
        }
    }

    for (int i = 1; i < numThreads; i++) {
        workers[i] = std::thread(
            runWorker, &args[i], workerFn, profileThreads, useBlockRanges);
    }

    runWorker(&args[0], workerFn, profileThreads, useBlockRanges);

    for (int i = 1; i < numThreads; i++) {
        workers[i].join();
    }
}

void mandelbrotThreadWithStrategy(
    int numThreads,
    float x0, float y0, float x1, float y1,
    int width, int height,
    int maxIterations, int output[],
    PartitionStrategy strategy,
    bool profileThreads) {
    switch (strategy) {
    case PARTITION_BLOCK:
        mandelbrotThreadImpl(
            numThreads, x0, y0, x1, y1, width, height, maxIterations, output,
            workerThreadStart_Block, true, profileThreads);
        break;
    case PARTITION_INTERLEAVED:
        mandelbrotThreadImpl(
            numThreads, x0, y0, x1, y1, width, height, maxIterations, output,
            workerThreadStart_Interleaved, false, profileThreads);
        break;
    default:
        fprintf(stderr, "Unknown partition strategy\n");
        exit(1);
    }
}

void mandelbrotThread(
    int numThreads,
    float x0, float y0, float x1, float y1,
    int width, int height,
    int maxIterations, int output[]) {
    mandelbrotThreadWithStrategy(
        numThreads, x0, y0, x1, y1, width, height, maxIterations, output,
        PARTITION_INTERLEAVED, false);
}
