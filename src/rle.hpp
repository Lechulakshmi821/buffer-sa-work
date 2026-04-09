#pragma once
#include <vector>
#include <cstdint>

struct RLEPair {
    uint32_t value;
    uint32_t count;
};

std::vector<RLEPair> rle_compress(const std::vector<uint32_t>& input);
std::vector<uint32_t> rle_decompress_cpu(const std::vector<RLEPair>& input);

// GPU version
std::vector<uint32_t> rle_decompress_gpu(const std::vector<RLEPair>& input, double& seconds);
