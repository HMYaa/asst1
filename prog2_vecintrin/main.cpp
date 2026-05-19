#include <stdio.h>
#include <algorithm>
#include <getopt.h>
#include <math.h>
#include "CS149intrin.h"
#include "logger.h"
using namespace std;

#define EXP_MAX 10

Logger CS149Logger;

void usage(const char* progname);
void initValue(float* values, int* exponents, float* output, float* gold, unsigned int N);
void absSerial(float* values, float* output, int N);
void absVector(float* values, float* output, int N);
void clampedExpSerial(float* values, int* exponents, float* output, int N);
void clampedExpVector(float* values, int* exponents, float* output, int N);
float arraySumSerial(float* values, int N);
float arraySumVector(float* values, int N);
bool verifyResult(float* values, int* exponents, float* output, float* gold, int N);

int main(int argc, char * argv[]) {
  int N = 16;
  bool printLog = false;

  // parse commandline options ////////////////////////////////////////////
  int opt;
  static struct option long_options[] = {
    {"size", 1, 0, 's'},
    {"log", 0, 0, 'l'},
    {"help", 0, 0, '?'},
    {0 ,0, 0, 0}
  };

  while ((opt = getopt_long(argc, argv, "s:l?", long_options, NULL)) != EOF) {

    switch (opt) {
      case 's':
        N = atoi(optarg);
        if (N <= 0) {
          printf("Error: Workload size is set to %d (<0).\n", N);
          return -1;
        }
        break;
      case 'l':
        printLog = true;
        break;
      case '?':
      default:
        usage(argv[0]);
        return 1;
    }
  }


  float* values = new float[N+VECTOR_WIDTH];
  int* exponents = new int[N+VECTOR_WIDTH];
  float* output = new float[N+VECTOR_WIDTH];
  float* gold = new float[N+VECTOR_WIDTH];
  initValue(values, exponents, output, gold, N);

  clampedExpSerial(values, exponents, gold, N);
  clampedExpVector(values, exponents, output, N);

  //absSerial(values, gold, N);
  //absVector(values, output, N);

  printf("\e[1;31mCLAMPED EXPONENT\e[0m (required) \n");
  bool clampedCorrect = verifyResult(values, exponents, output, gold, N);
  if (printLog) CS149Logger.printLog();
  CS149Logger.printStats();

  printf("************************ Result Verification *************************\n");
  if (!clampedCorrect) {
    printf("@@@ Failed!!!\n");
  } else {
    printf("Passed!!!\n");
  }

  printf("\n\e[1;31mARRAY SUM\e[0m (bonus) \n");
  if (N % VECTOR_WIDTH == 0) {
    float sumGold = arraySumSerial(values, N);
    float sumOutput = arraySumVector(values, N);
    float epsilon = 0.1;
    bool sumCorrect = abs(sumGold - sumOutput) < epsilon * 2;
    if (!sumCorrect) {
      printf("Expected %f, got %f\n.", sumGold, sumOutput);
      printf("@@@ Failed!!!\n");
    } else {
      printf("Passed!!!\n");
    }
  } else {
    printf("Must have N %% VECTOR_WIDTH == 0 for this problem (VECTOR_WIDTH is %d)\n", VECTOR_WIDTH);
  }

  delete [] values;
  delete [] exponents;
  delete [] output;
  delete [] gold;

  return 0;
}

void usage(const char* progname) {
  printf("Usage: %s [options]\n", progname);
  printf("Program Options:\n");
  printf("  -s  --size <N>     Use workload size N (Default = 16)\n");
  printf("  -l  --log          Print vector unit execution log\n");
  printf("  -?  --help         This message\n");
}

void initValue(float* values, int* exponents, float* output, float* gold, unsigned int N) {

  for (unsigned int i=0; i<N+VECTOR_WIDTH; i++)
  {
    // random input values
    values[i] = -1.f + 4.f * static_cast<float>(rand()) / RAND_MAX;
    exponents[i] = rand() % EXP_MAX;
    output[i] = 0.f;
    gold[i] = 0.f;
  }

}

bool verifyResult(float* values, int* exponents, float* output, float* gold, int N) {
  int incorrect = -1;
  float epsilon = 0.00001;
  for (int i=0; i<N+VECTOR_WIDTH; i++) {
    if ( abs(output[i] - gold[i]) > epsilon ) {
      incorrect = i;
      break;
    }
  }

  if (incorrect != -1) {
    if (incorrect >= N)
      printf("You have written to out of bound value!\n");
    printf("Wrong calculation at value[%d]!\n", incorrect);
    printf("value  = ");
    for (int i=0; i<N; i++) {
      printf("% f ", values[i]);
    } printf("\n");

    printf("exp    = ");
    for (int i=0; i<N; i++) {
      printf("% 9d ", exponents[i]);
    } printf("\n");

    printf("output = ");
    for (int i=0; i<N; i++) {
      printf("% f ", output[i]);
    } printf("\n");

    printf("gold   = ");
    for (int i=0; i<N; i++) {
      printf("% f ", gold[i]);
    } printf("\n");
    return false;
  }
  printf("Results matched with answer!\n");
  return true;
}

// computes the absolute value of all elements in the input array
// values, stores result in output
void absSerial(float* values, float* output, int N) {
  for (int i=0; i<N; i++) {
    float x = values[i];
    if (x < 0) {
      output[i] = -x;
    } else {
      output[i] = x;
    }
  }
}


// implementation of absSerial() above, but it is vectorized using CS149 intrinsics
void absVector(float* values, float* output, int N) {
  __cs149_vec_float x;
  __cs149_vec_float result;
  __cs149_vec_float zero = _cs149_vset_float(0.f);
  __cs149_mask maskAll, maskIsNegative, maskIsNotNegative;

//  Note: Take a careful look at this loop indexing.  This example
//  code is not guaranteed to work when (N % VECTOR_WIDTH) != 0.
//  Why is that the case?
  for (int i=0; i<N; i+=VECTOR_WIDTH) {

    // All ones
    maskAll = _cs149_init_ones();

    // All zeros
    maskIsNegative = _cs149_init_ones(0);

    // Load vector of values from contiguous memory addresses
    _cs149_vload_float(x, values+i, maskAll);               // x = values[i];

    // Set mask according to predicate
    _cs149_vlt_float(maskIsNegative, x, zero, maskAll);     // if (x < 0) {

    // Execute instruction using mask ("if" clause)
    _cs149_vsub_float(result, zero, x, maskIsNegative);      //   output[i] = -x;

    // Inverse maskIsNegative to generate "else" mask
    maskIsNotNegative = _cs149_mask_not(maskIsNegative);     // } else {

    // Execute instruction ("else" clause)
    _cs149_vload_float(result, values+i, maskIsNotNegative); //   output[i] = x; }

    // Write results back to memory
    _cs149_vstore_float(output+i, result, maskAll);
  }
}


// accepts an array of values and an array of exponents
//
// For each element, compute values[i]^exponents[i] and clamp value to
// 9.999.  Store result in output.
void clampedExpSerial(float* values, int* exponents, float* output, int N) {
  for (int i=0; i<N; i++) {
    float x = values[i];
    int y = exponents[i];
    if (y == 0) {
      output[i] = 1.f;
    } else {
      float result = x;
      int count = y - 1;
      while (count > 0) {
        result *= x;
        count--;
      }
      if (result > 9.999999f) {
        result = 9.999999f;
      }
      output[i] = result;
    }
  }
}

// 编译时可改版本：1=动态 while，2=固定 9 轮乘方，3=固定轮次+精简 count（默认）
#ifndef CLAMPED_EXP_VERSION
#define CLAMPED_EXP_VERSION 3
#endif

// 加分 arraySum：1=向量累加后标量归约，2=hadd+interleave 树形归约（默认）
#ifndef ARRAY_SUM_VERSION
#define ARRAY_SUM_VERSION 2
#endif

// v1：基础实现。内层用 while+cntbits，指数小时提前退出，但总向量指令更多。
static void clampedExpVector_v1(float* values, int* exponents, float* output, int N) {
  __cs149_vec_float x, result;
  __cs149_vec_int exp, count;
  __cs149_mask maskAll, maskZero, maskNonZero, maskActive, maskClamp;
  __cs149_vec_float clampVal = _cs149_vset_float(9.999999f);
  __cs149_vec_int zeroI = _cs149_vset_int(0);
  __cs149_vec_int oneI = _cs149_vset_int(1);

  for (int i = 0; i < N; i += VECTOR_WIDTH) {
    // 尾部不足一整向量时，只启用前 rem 个 lane（对应 ./myexp -s 3）
    maskAll = _cs149_init_ones(std::min(VECTOR_WIDTH, N - i));

    _cs149_vload_float(x, values + i, maskAll);
    _cs149_vload_int(exp, exponents + i, maskAll);

    // exponent==0 → 结果为 1；否则先从 x 开始乘方
    _cs149_veq_int(maskZero, exp, zeroI, maskAll);
    maskNonZero = _cs149_mask_not(maskZero);
    _cs149_vmove_float(result, x, maskAll);
    _cs149_vset_float(result, 1.f, maskZero);

    // count = exponent-1，表示还要乘几次 x（与串行 while(count>0) 一致）
    _cs149_vset_int(count, 0, maskAll);
    _cs149_vsub_int(count, exp, oneI, maskNonZero);

    // 各 lane 的 count 不同，用 mask 模拟 while；无活跃 lane 时退出
    while (true) {
      _cs149_vgt_int(maskActive, count, zeroI, maskAll);
      if (_cs149_cntbits(maskActive) == 0) {
        break;
      }
      _cs149_vmult_float(result, result, x, maskActive);
      _cs149_vsub_int(count, count, oneI, maskActive);
    }

    // 上限 clamp 到 9.999999
    _cs149_vgt_float(maskClamp, result, clampVal, maskAll);
    _cs149_vset_float(result, 9.999999f, maskClamp);
    _cs149_vstore_float(output + i, result, maskAll);
  }
}

// v2：去掉 cntbits，固定跑 EXP_MAX-1(=9) 轮；指令更少，但指数小时有空转 lane。
static void clampedExpVector_v2(float* values, int* exponents, float* output, int N) {
  __cs149_vec_float x, result;
  __cs149_vec_int exp, count;
  __cs149_mask maskAll, maskZero, maskNonZero, maskActive, maskClamp;
  __cs149_vec_float clampVal = _cs149_vset_float(9.999999f);
  __cs149_vec_int zeroI = _cs149_vset_int(0);
  __cs149_vec_int oneI = _cs149_vset_int(1);

  for (int i = 0; i < N; i += VECTOR_WIDTH) {
    maskAll = _cs149_init_ones(std::min(VECTOR_WIDTH, N - i));

    _cs149_vload_float(x, values + i, maskAll);
    _cs149_vload_int(exp, exponents + i, maskAll);

    _cs149_veq_int(maskZero, exp, zeroI, maskAll);
    maskNonZero = _cs149_mask_not(maskZero);
    _cs149_vmove_float(result, x, maskAll);
    _cs149_vset_float(result, 1.f, maskZero);

    _cs149_vset_int(count, 0, maskAll);
    _cs149_vsub_int(count, exp, oneI, maskNonZero);

    // 最多乘 9 次（exponent<EXP_MAX），每轮仅 count>0 的 lane 参与
    for (int iter = 0; iter < EXP_MAX - 1; iter++) {
      _cs149_vgt_int(maskActive, count, zeroI, maskAll);
      _cs149_vmult_float(result, result, x, maskActive);
      _cs149_vsub_int(count, count, oneI, maskActive);
    }

    _cs149_vgt_float(maskClamp, result, clampVal, maskAll);
    _cs149_vset_float(result, 9.999999f, maskClamp);
    _cs149_vstore_float(output + i, result, maskAll);
  }
}

// v3（默认）：同 v2 固定轮次；count 由 exp 拷贝再减 1，exp==0 时 count 保持 0。
static void clampedExpVector_v3(float* values, int* exponents, float* output, int N) {
  __cs149_vec_float x, result;
  __cs149_vec_int exp, count;
  __cs149_mask maskAll, maskZero, maskNonZero, maskActive, maskClamp;
  __cs149_vec_float clampVal = _cs149_vset_float(9.999999f);
  __cs149_vec_int zeroI = _cs149_vset_int(0);
  __cs149_vec_int oneI = _cs149_vset_int(1);

  for (int i = 0; i < N; i += VECTOR_WIDTH) {
    maskAll = _cs149_init_ones(std::min(VECTOR_WIDTH, N - i));

    _cs149_vload_float(x, values + i, maskAll);
    _cs149_vload_int(exp, exponents + i, maskAll);

    _cs149_veq_int(maskZero, exp, zeroI, maskAll);
    maskNonZero = _cs149_mask_not(maskZero);
    _cs149_vmove_float(result, x, maskAll);
    _cs149_vset_float(result, 1.f, maskZero);
    // exp==0 → count 保持 0；否则 count=exp-1（还要乘几次 x）
    _cs149_vmove_int(count, exp, maskAll);
    _cs149_vsub_int(count, count, oneI, maskNonZero);

    for (int iter = 0; iter < EXP_MAX - 1; iter++) {
      _cs149_vgt_int(maskActive, count, zeroI, maskAll);
      _cs149_vmult_float(result, result, x, maskActive);
      _cs149_vsub_int(count, count, oneI, maskActive);
    }

    _cs149_vgt_float(maskClamp, result, clampVal, maskAll);
    _cs149_vset_float(result, 9.999999f, maskClamp);
    _cs149_vstore_float(output + i, result, maskAll);
  }
}

// 入口：按 CLAMPED_EXP_VERSION 分发到对应实现
void clampedExpVector(float* values, int* exponents, float* output, int N) {
#if CLAMPED_EXP_VERSION == 1
  clampedExpVector_v1(values, exponents, output, N);
#elif CLAMPED_EXP_VERSION == 2
  clampedExpVector_v2(values, exponents, output, N);
#else
  clampedExpVector_v3(values, exponents, output, N);
#endif
}

// returns the sum of all elements in values
float arraySumSerial(float* values, int N) {
  float sum = 0;
  for (int i=0; i<N; i++) {
    sum += values[i];
  }

  return sum;
}

// returns the sum of all elements in values
// You can assume N is a multiple of VECTOR_WIDTH
// You can assume VECTOR_WIDTH is a power of 2
// v1：每块向量加到 sumVec，各 lane 存的是「同余位置」的部分和，最后标量相加。
static float arraySumVector_v1(float* values, int N) {
  __cs149_vec_float sumVec = _cs149_vset_float(0.f);
  __cs149_mask maskAll = _cs149_init_ones();

  for (int i = 0; i < N; i += VECTOR_WIDTH) {
    __cs149_vec_float v;
    _cs149_vload_float(v, values + i, maskAll);
    _cs149_vadd_float(sumVec, sumVec, v, maskAll);
  }

  // sumVec[j] = values[j] + values[j+W] + ...，需把 W 个 lane 再加成标量
  float sum = 0.f;
  for (int j = 0; j < VECTOR_WIDTH; j++) {
    sum += sumVec.value[j];
  }
  return sum;
}

// v2：寄存器内用 hadd 合并相邻 lane，interleave 调整布局，约 log2(W) 步归约。
static float arraySumVector_v2(float* values, int N) {
  __cs149_vec_float sumVec = _cs149_vset_float(0.f);
  __cs149_mask maskAll = _cs149_init_ones();

  for (int i = 0; i < N; i += VECTOR_WIDTH) {
    __cs149_vec_float v;
    _cs149_vload_float(v, values + i, maskAll);
    _cs149_vadd_float(sumVec, sumVec, v, maskAll);
  }

  __cs149_vec_float tmp;
  for (int stride = VECTOR_WIDTH / 2; stride >= 1; stride /= 2) {
    _cs149_hadd_float(tmp, sumVec);           // 相邻 lane 相加，如 [a,b,c,d]→[a+b,a+b,c+d,c+d]
    if (stride > 1) {
      _cs149_interleave_float(sumVec, tmp);   // 把部分和挪到可继续 hadd 的位置
    } else {
      _cs149_vmove_float(sumVec, tmp, maskAll); // 最后一轮：总和已在 lane 0
    }
  }
  return sumVec.value[0];
}

float arraySumVector(float* values, int N) {
#if ARRAY_SUM_VERSION == 1
  return arraySumVector_v1(values, N);
#else
  return arraySumVector_v2(values, N);
#endif
}

