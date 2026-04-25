#include <iostream>
#include <vector>
#include <cstdint>
#include <chrono>
#include <iomanip>
#include <string>
#include <fstream>
#include <cstdlib>

#include "rle.hpp"

// dataset generators
std::vector<uint32_t> generate_repeated(size_t N);
std::vector<uint32_t> generate_uniform(size_t N);
std::vector<uint32_t> generate_sorted(size_t N);
std::vector<uint32_t> generate_skewed(size_t N);

struct Result {
    std::string ratio_label;
    double avg_total_mbps;
    double min_total_mbps;
    double max_total_mbps;
};

bool verify_equal(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); i++) {
        if (a[i] != b[i]) return false;
    }
    return true;
}

std::vector<uint32_t> concat_vectors(const std::vector<uint32_t>& a,
                                     const std::vector<uint32_t>& b) {
    std::vector<uint32_t> out;
    out.reserve(a.size() + b.size());
    out.insert(out.end(), a.begin(), a.end());
    out.insert(out.end(), b.begin(), b.end());
    return out;
}

// Debug benchmark with warm-up and detailed timing
Result run_ratio_benchmark_debug(const std::vector<uint32_t>& data, int cpu_percent) {
    const int NUM_RUNS = 10;

    // compress once on CPU
    std::vector<RLEPair> compressed = rle_compress(data);

    std::size_t total_pairs = compressed.size();
    std::size_t cpu_pairs = (total_pairs * cpu_percent) / 100;
    std::size_t gpu_pairs = total_pairs - cpu_pairs;

    std::vector<RLEPair> cpu_part(compressed.begin(), compressed.begin() + cpu_pairs);
    std::vector<RLEPair> gpu_part(compressed.begin() + cpu_pairs, compressed.end());

    double original_bytes = static_cast<double>(data.size() * sizeof(uint32_t));

    std::cout << "\n====================================================\n";
    std::cout << "CPU:GPU ratio = " << cpu_percent << ":" << (100 - cpu_percent) << "\n";
    std::cout << "Total compressed pairs = " << total_pairs << "\n";
    std::cout << "CPU pairs = " << cpu_pairs << ", GPU pairs = " << gpu_pairs << "\n";
    std::cout << "====================================================\n";

    // GPU warm-up (very important)
    if (!gpu_part.empty()) {
        double warmup_gpu_time = 0.0;
        std::vector<uint32_t> warmup_out = rle_decompress_gpu(gpu_part, warmup_gpu_time);
        std::cout << "Warm-up GPU run done. GPU internal time = "
                  << warmup_gpu_time << " s\n";
    }

    std::vector<double> total_mbps_values;

    for (int run = 0; run < NUM_RUNS; run++) {
        std::vector<uint32_t> cpu_output;
        std::vector<uint32_t> gpu_output;

        // total timing
        auto total_start = std::chrono::high_resolution_clock::now();

        // CPU timing
        auto cpu_start = std::chrono::high_resolution_clock::now();
        if (!cpu_part.empty()) {
            cpu_output = rle_decompress_cpu(cpu_part);
        }
        auto cpu_end = std::chrono::high_resolution_clock::now();
        double cpu_seconds = std::chrono::duration<double>(cpu_end - cpu_start).count();

        // GPU timing (this already includes transfer + GPU decompress + copy back)
        double gpu_seconds = 0.0;
        if (!gpu_part.empty()) {
            gpu_output = rle_decompress_gpu(gpu_part, gpu_seconds);
        }

        // combine
        std::vector<uint32_t> final_output = concat_vectors(cpu_output, gpu_output);

        auto total_end = std::chrono::high_resolution_clock::now();
        double total_seconds = std::chrono::duration<double>(total_end - total_start).count();

        // verify only once
        if (run == 0) {
            bool ok = verify_equal(data, final_output);
            std::cout << "Verification: " << (ok ? "PASS" : "FAIL") << "\n";
        }

        double total_mbps = (original_bytes / (1024.0 * 1024.0)) / total_seconds;
        total_mbps_values.push_back(total_mbps);

        std::cout << "Run " << (run + 1)
                  << " | CPU time = " << cpu_seconds << " s"
                  << " | GPU path time = " << gpu_seconds << " s"
                  << " | Total time = " << total_seconds << " s"
                  << " | Total throughput = " << total_mbps << " MB/s"
                  << "\n";
    }

    // average / min / max
    double sum = 0.0;
    for (double v : total_mbps_values) sum += v;
    double avg = sum / NUM_RUNS;

    double min_val = total_mbps_values[0];
    double max_val = total_mbps_values[0];

    for (double v : total_mbps_values) {
        if (v < min_val) min_val = v;
        if (v > max_val) max_val = v;
    }

    std::cout << "Average throughput = " << avg << " MB/s\n";
    std::cout << "Min throughput = " << min_val << " MB/s\n";
    std::cout << "Max throughput = " << max_val << " MB/s\n";

    Result r;
    r.ratio_label = std::to_string(cpu_percent) + ":" + std::to_string(100 - cpu_percent);
    r.avg_total_mbps = avg;
    r.min_total_mbps = min_val;
    r.max_total_mbps = max_val;

    return r;
}

void print_table(const std::vector<Result>& results) {
    std::cout << "\n-----------------------------------------------------------------\n";
    std::cout << "| CPU:GPU Ratio | Avg MB/s | Min MB/s | Max MB/s |\n";
    std::cout << "-----------------------------------------------------------------\n";

    for (const auto& r : results) {
        std::cout << "| "
                  << std::setw(13) << std::left << r.ratio_label
                  << "| "
                  << std::setw(9) << std::left << std::setprecision(4) << r.avg_total_mbps
                  << "| "
                  << std::setw(9) << std::left << std::setprecision(4) << r.min_total_mbps
                  << "| "
                  << std::setw(9) << std::left << std::setprecision(4) << r.max_total_mbps
                  << "|\n";
    }

    std::cout << "-----------------------------------------------------------------\n";
}

void save_csv(const std::vector<Result>& results) {
    std::ofstream file("ratio_results_debug.csv");
    file << "CPU_GPU_Ratio,Avg_MBps,Min_MBps,Max_MBps\n";

    for (const auto& r : results) {
        file << r.ratio_label << ","
             << r.avg_total_mbps << ","
             << r.min_total_mbps << ","
             << r.max_total_mbps << "\n";
    }

    file.close();
    std::cout << "\nResults saved to ratio_results_debug.csv\n";
}

int main() {
    std::size_t N = 10000000;

    // keep one dataset fixed
    std::vector<uint32_t> data = generate_skewed(N);

    std::vector<Result> results;

    // You can run only 75:25 first for debugging:
    // results.push_back(run_ratio_benchmark_debug(data, 75));

    // Or all ratios:
    results.push_back(run_ratio_benchmark_debug(data, 100));
    results.push_back(run_ratio_benchmark_debug(data, 75));
    results.push_back(run_ratio_benchmark_debug(data, 50));
    results.push_back(run_ratio_benchmark_debug(data, 25));
    results.push_back(run_ratio_benchmark_debug(data, 0));

    print_table(results);
    save_csv(results);

    return 0;
}
