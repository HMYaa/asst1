#include <stdio.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <pthread.h>
#include <math.h>

#include "CycleTimer.h"
#include "sqrt_ispc.h"

using namespace ispc;

extern void sqrtSerial(int N, float startGuess, float* values, float* output);

static void verifyResult(int N, float* result, float* gold) {
    for (int i=0; i<N; i++) {
        if (fabs(result[i] - gold[i]) > 1e-4) {
            printf("Error: [%d] Got %f expected %f\n", i, result[i], gold[i]);
        }
    }
}

enum InputMode {
    INPUT_RANDOM,
    INPUT_BEST,
    INPUT_WORST
};

static InputMode parseInputMode(int argc, char** argv) {
    if (argc < 2 || strcmp(argv[1], "random") == 0) {
        return INPUT_RANDOM;
    }
    if (strcmp(argv[1], "best") == 0) {
        return INPUT_BEST;
    }
    if (strcmp(argv[1], "worst") == 0) {
        return INPUT_WORST;
    }

    printf("Unknown input mode '%s'. Use random, best, or worst.\n", argv[1]);
    exit(1);
}

static const char* inputModeName(InputMode mode) {
    switch (mode) {
        case INPUT_RANDOM: return "random";
        case INPUT_BEST: return "best";
        case INPUT_WORST: return "worst";
    }
    return "unknown";
}

static float inputValueForMode(InputMode mode, unsigned int i) {
    const float slowConvergingValue = 2.999f;

    switch (mode) {
        case INPUT_RANDOM:
            return .001f + 2.998f * static_cast<float>(rand()) / RAND_MAX;
        case INPUT_BEST:
            return slowConvergingValue;
        case INPUT_WORST:
            return (i % 8 == 0) ? slowConvergingValue : 1.f;
    }
    return 1.f;
}

int main(int argc, char** argv) {

    const unsigned int N = 20 * 1000 * 1000;
    const float initialGuess = 1.0f;
    InputMode inputMode = parseInputMode(argc, argv);

    float* values = new float[N];
    float* output = new float[N];
    float* gold = new float[N];

    for (unsigned int i=0; i<N; i++)
    {
        values[i] = inputValueForMode(inputMode, i);
    }

    printf("Input mode: %s\n", inputModeName(inputMode));

    // generate a gold version to check results
    for (unsigned int i=0; i<N; i++)
        gold[i] = sqrt(values[i]);

    //
    // And run the serial implementation 3 times, again reporting the
    // minimum time.
    //
    double minSerial = 1e30;
    for (int i = 0; i < 3; ++i) {
        double startTime = CycleTimer::currentSeconds();
        sqrtSerial(N, initialGuess, values, output);
        double endTime = CycleTimer::currentSeconds();
        minSerial = std::min(minSerial, endTime - startTime);
    }

    printf("[sqrt serial]:\t\t[%.3f] ms\n", minSerial * 1000);

    verifyResult(N, output, gold);

    //
    // Compute the image using the ispc implementation; report the minimum
    // time of three runs.
    //
    double minISPC = 1e30;
    for (int i = 0; i < 3; ++i) {
        double startTime = CycleTimer::currentSeconds();
        sqrt_ispc(N, initialGuess, values, output);
        double endTime = CycleTimer::currentSeconds();
        minISPC = std::min(minISPC, endTime - startTime);
    }

    printf("[sqrt ispc]:\t\t[%.3f] ms\n", minISPC * 1000);

    verifyResult(N, output, gold);

    // Clear out the buffer
    for (unsigned int i = 0; i < N; ++i)
        output[i] = 0;

    //
    // Tasking version of the ISPC code
    //
    double minTaskISPC = 1e30;
    for (int i = 0; i < 3; ++i) {
        double startTime = CycleTimer::currentSeconds();
        sqrt_ispc_withtasks(N, initialGuess, values, output);
        double endTime = CycleTimer::currentSeconds();
        minTaskISPC = std::min(minTaskISPC, endTime - startTime);
    }

    printf("[sqrt task ispc]:\t[%.3f] ms\n", minTaskISPC * 1000);

    verifyResult(N, output, gold);

    printf("\t\t\t\t(%.2fx speedup from ISPC)\n", minSerial/minISPC);
    printf("\t\t\t\t(%.2fx speedup from task ISPC)\n", minSerial/minTaskISPC);

    delete [] values;
    delete [] output;
    delete [] gold;

    return 0;
}
