// Windows host runner for the RTX 5090 issue-rate probe kernels, run as-is
// against this machine's RTX 5080 (also sm_120). Simplified vs. the Linux
// tool (no NVRTC/driver-API/cubin-hash pinning -- nvcc on Windows builds the
// .cu directly), same device kernels, same instruction sequences, same
// inputs. Prints one measured TFLOP/s (or TOPS for u4) per mode, best of N
// reps, plus a basic non-zero/non-NaN sanity check (not full per-cell
// validation like the reference tool -- this is a quick confirmatory
// measurement, not the published-grade artifact).

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cuda_runtime.h>

extern "C" __global__ void bench_f16_f32(unsigned long long iters, float *sink);
extern "C" __global__ void bench_bf16_f32(unsigned long long iters, float *sink);
extern "C" __global__ void bench_f16_f16(unsigned long long iters, float *sink);
extern "C" __global__ void bench_tf32(unsigned long long iters, float *sink);
extern "C" __global__ void bench_u4(unsigned long long iters, int *sink);

#define CHAINS 8

#define CUDA_CHECK(x) do { cudaError_t e_ = (x); if (e_ != cudaSuccess) { \
    fprintf(stderr, "CUDA error %s at %s:%d: %s\n", #x, __FILE__, __LINE__, \
            cudaGetErrorString(e_)); exit(1); } } while (0)

struct ModeResult {
    const char *name;
    double best_rate; // TFLOP/s or TOPS
    bool is_tops;
};

static double now_rate(double flops_per_iter_total, float ms) {
    double seconds = ms / 1000.0;
    return (flops_per_iter_total / seconds) / 1e12;
}

int main(int argc, char **argv) {
    unsigned long long iters = 20000;
    int reps = 3;
    if (argc > 1) iters = strtoull(argv[1], nullptr, 10);
    if (argc > 2) reps = atoi(argv[2]);

    cudaDeviceProp prop;
    CUDA_CHECK(cudaGetDeviceProperties(&prop, 0));
    int blocks = prop.multiProcessorCount * 4;
    int threads = 256;
    int warps_per_block = threads / 32;

    printf("{\n");
    printf("  \"gpu\": \"%s\", \"sm_count\": %d, \"blocks\": %d, \"threads\": %d, "
           "\"chains\": %d, \"iters\": %llu, \"reps\": %d,\n",
           prop.name, prop.multiProcessorCount, blocks, threads, CHAINS, iters, reps);

    float *sink_f;
    int *sink_i;
    CUDA_CHECK(cudaMalloc(&sink_f, (size_t)blocks * threads * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&sink_i, (size_t)blocks * threads * sizeof(int)));

    cudaEvent_t start, stop;
    CUDA_CHECK(cudaEventCreate(&start));
    CUDA_CHECK(cudaEventCreate(&stop));

    // flops per mma instruction: m16n8k16 => 16*8*16*2 = 4096; m16n8k8 => 2048;
    // m16n8k64 (int4) => 16*8*64*2 = 16384 (ops, reported as TOPS not TFLOPs).
    // total mma issues = iters * CHAINS * warps_per_block * blocks (one mma
    // per warp per chain per iteration -- c0..c3/c0..c1 are per-thread
    // accumulator registers of the *same* mma, not separate instructions).
    double total_mmas = (double)iters * CHAINS * warps_per_block * blocks;

    struct { const char *name; void (*fn)(unsigned long long, float*); double flops_per_mma;
             unsigned long long iters_override; bool is_int; } modes_f[] = {
        {"f16_f32",  bench_f16_f32,  4096.0, 0, false},
        {"bf16_f32", bench_bf16_f32, 4096.0, 0, false},
        {"f16_f16",  bench_f16_f16,  4096.0, 2000, false},
        {"tf32",     bench_tf32,     2048.0, 0, false},
    };

    printf("  \"modes\": {\n");
    for (size_t m = 0; m < sizeof(modes_f)/sizeof(modes_f[0]); ++m) {
        unsigned long long it = modes_f[m].iters_override ? modes_f[m].iters_override : iters;
        double this_total_mmas = (double)it * CHAINS * warps_per_block * blocks;
        double flops_total = this_total_mmas * modes_f[m].flops_per_mma;
        double best_ms = 1e18;
        for (int r = 0; r < reps; ++r) {
            CUDA_CHECK(cudaEventRecord(start));
            modes_f[m].fn<<<blocks, threads>>>(it, sink_f);
            CUDA_CHECK(cudaEventRecord(stop));
            CUDA_CHECK(cudaEventSynchronize(stop));
            cudaError_t launch_err = cudaGetLastError();
            if (launch_err != cudaSuccess) {
                fprintf(stderr, "launch error (%s): %s\n", modes_f[m].name,
                        cudaGetErrorString(launch_err));
                exit(1);
            }
            float ms = 0;
            CUDA_CHECK(cudaEventElapsedTime(&ms, start, stop));
            if (ms < best_ms) best_ms = ms;
        }
        double rate = now_rate(flops_total, best_ms);

        // basic sanity: pull back one cell, check finite and non-zero
        float sample;
        CUDA_CHECK(cudaMemcpy(&sample, sink_f, sizeof(float), cudaMemcpyDeviceToHost));
        bool ok = std::isfinite(sample) && sample != 0.0f;

        printf("    \"%s\": {\"best_Top_per_s\": %.2f, \"best_ms\": %.3f, "
               "\"sample_cell\": %.4f, \"sane\": %s}%s\n",
               modes_f[m].name, rate, best_ms, sample, ok ? "true" : "false",
               (m + 1 < sizeof(modes_f)/sizeof(modes_f[0])) ? "," : "");
    }

    // u4 mode: integer, separate sink
    {
        double flops_per_mma = 16384.0;
        double flops_total = total_mmas * flops_per_mma;
        double best_ms = 1e18;
        for (int r = 0; r < reps; ++r) {
            CUDA_CHECK(cudaEventRecord(start));
            bench_u4<<<blocks, threads>>>(iters, sink_i);
            CUDA_CHECK(cudaEventRecord(stop));
            CUDA_CHECK(cudaEventSynchronize(stop));
            cudaError_t launch_err = cudaGetLastError();
            if (launch_err != cudaSuccess) {
                fprintf(stderr, "launch error (u4_s32): %s\n", cudaGetErrorString(launch_err));
                exit(1);
            }
            float ms = 0;
            CUDA_CHECK(cudaEventElapsedTime(&ms, start, stop));
            if (ms < best_ms) best_ms = ms;
        }
        double rate = now_rate(flops_total, best_ms);
        int sample;
        CUDA_CHECK(cudaMemcpy(&sample, sink_i, sizeof(int), cudaMemcpyDeviceToHost));
        bool ok = (sample != 0);
        printf("    ,\"u4_s32\": {\"best_Top_per_s\": %.2f, \"best_ms\": %.3f, "
               "\"sample_cell\": %d, \"sane\": %s}\n", rate, best_ms, sample, ok ? "true" : "false");
    }

    printf("  }\n");
    printf("}\n");

    cudaFree(sink_f);
    cudaFree(sink_i);
    return 0;
}
