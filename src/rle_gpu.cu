#include "rle.hpp"
#include <cuda_runtime.h>
#include <iostream>
#include <vector>

#define CUDA_CHECK(call)                                                   \
    do {                                                                   \
        cudaError_t err = call;                                            \
        if (err != cudaSuccess) {                                          \
            std::cerr << "CUDA error: " << cudaGetErrorString(err)         \
                      << " at line " << __LINE__ << std::endl;             \
            std::exit(1);                                                  \
        }                                                                  \
    } while (0)

__global__ void rle_decompress_kernel(const RLEPair* pairs,
                                      const uint32_t* offsets,
                                      size_t num_pairs,
                                      uint32_t* out) {
    size_t i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= num_pairs) return;

    uint32_t value = pairs[i].value;
    uint32_t count = pairs[i].count;
    uint32_t start = offsets[i];

    for (uint32_t j = 0; j < count; j++) {
        out[start + j] = value;
    }
}

std::vector<uint32_t> rle_decompress_gpu(const std::vector<RLEPair>& input, double& seconds) {
    size_t num_pairs = input.size();

    // compute output size + offsets on CPU
    std::vector<uint32_t> offsets(num_pairs);
    size_t total_size = 0;
    for (size_t i = 0; i < num_pairs; i++) {
        offsets[i] = static_cast<uint32_t>(total_size);
        total_size += input[i].count;
    }

    std::vector<uint32_t> output(total_size);

    RLEPair* d_pairs = nullptr;
    uint32_t* d_offsets = nullptr;
    uint32_t* d_out = nullptr;

    CUDA_CHECK(cudaMalloc(&d_pairs, num_pairs * sizeof(RLEPair)));
    CUDA_CHECK(cudaMalloc(&d_offsets, num_pairs * sizeof(uint32_t)));
    CUDA_CHECK(cudaMalloc(&d_out, total_size * sizeof(uint32_t)));

    cudaEvent_t start, stop;
    CUDA_CHECK(cudaEventCreate(&start));
    CUDA_CHECK(cudaEventCreate(&stop));

    CUDA_CHECK(cudaEventRecord(start));

    CUDA_CHECK(cudaMemcpy(d_pairs, input.data(),
                          num_pairs * sizeof(RLEPair),
                          cudaMemcpyHostToDevice));

    CUDA_CHECK(cudaMemcpy(d_offsets, offsets.data(),
                          num_pairs * sizeof(uint32_t),
                          cudaMemcpyHostToDevice));

    int blockSize = 256;
    int gridSize = static_cast<int>((num_pairs + blockSize - 1) / blockSize);

    rle_decompress_kernel<<<gridSize, blockSize>>>(d_pairs, d_offsets, num_pairs, d_out);
    CUDA_CHECK(cudaGetLastError());

    CUDA_CHECK(cudaMemcpy(output.data(), d_out,
                          total_size * sizeof(uint32_t),
                          cudaMemcpyDeviceToHost));

    CUDA_CHECK(cudaEventRecord(stop));
    CUDA_CHECK(cudaEventSynchronize(stop));

    float ms = 0.0f;
    CUDA_CHECK(cudaEventElapsedTime(&ms, start, stop));
    seconds = ms / 1000.0;

    CUDA_CHECK(cudaEventDestroy(start));
    CUDA_CHECK(cudaEventDestroy(stop));

    CUDA_CHECK(cudaFree(d_pairs));
    CUDA_CHECK(cudaFree(d_offsets));
    CUDA_CHECK(cudaFree(d_out));

    return output;
}
