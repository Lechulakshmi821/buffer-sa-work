#include <vector>
#include <random>
#include <cstdint>

std::vector<uint32_t> generate_repeated(size_t N) {
    std::vector<uint32_t> data(N);
    for (size_t i = 0; i < N; i++) {
        data[i] = 7;
    }
    return data;
}

std::vector<uint32_t> generate_uniform(size_t N) {
    std::vector<uint32_t> data(N);
    std::mt19937 rng(42);
    std::uniform_int_distribution<uint32_t> dist(0, 100000);

    for (size_t i = 0; i < N; i++) {
        data[i] = dist(rng);
    }
    return data;
}

std::vector<uint32_t> generate_sorted(size_t N) {
    std::vector<uint32_t> data(N);
    if (N == 0) return data;

    data[0] = 1000;
    for (size_t i = 1; i < N; i++) {
        data[i] = data[i - 1] + (i % 3);
    }
    return data;
}

std::vector<uint32_t> generate_skewed(size_t N) {
    std::vector<uint32_t> data(N);
    std::mt19937 rng(42);
    std::uniform_int_distribution<uint32_t> prob(0, 99);
    std::uniform_int_distribution<uint32_t> rare_val(2, 1000);

    for (size_t i = 0; i < N; i++) {
        if (prob(rng) < 80) {
            data[i] = 1;   // 80% same value
        } else {
            data[i] = rare_val(rng);
        }
    }
    return data;
}

