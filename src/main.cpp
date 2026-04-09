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
    double throughput_mbps;
    double throughput_rowsps;
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

// This is the mentor-style experiment:
// CPU compresses first, then part stays on CPU and part goes to GPU.
Result run_ratio_benchmark(const std::vector<uint32_t>& data, int cpu_percent) {
    // Step 1: compress on CPU
    std::vector<RLEPair> compressed = rle_compress(data);

    std::size_t total_pairs = compressed.size();
    std::size_t cpu_pairs = (total_pairs * cpu_percent) / 100;
    std::size_t gpu_pairs = total_pairs - cpu_pairs;

    std::vector<RLEPair> cpu_part(compressed.begin(), compressed.begin() + cpu_pairs);
    std::vector<RLEPair> gpu_part(compressed.begin() + cpu_pairs, compressed.end());

    double original_bytes = static_cast<double>(data.size() * sizeof(uint32_t));

    // Step 2: start total timing
    auto start = std::chrono::high_resolution_clock::now();

    // Step 3: CPU decompress its part
    std::vector<uint32_t> cpu_output;
    if (!cpu_part.empty()) {
        cpu_output = rle_decompress_cpu(cpu_part);
    }

    // Step 4: GPU gets compressed part, transfer+decompress measured inside GPU function
    std::vector<uint32_t> gpu_output;
    double gpu_seconds_internal = 0.0;
    if (!gpu_part.empty()) {
        gpu_output = rle_decompress_gpu(gpu_part, gpu_seconds_internal);
    }

    // Step 5: combine results
    std::vector<uint32_t> final_output = concat_vectors(cpu_output, gpu_output);

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> total_diff = end - start;
    double total_seconds = total_diff.count();

    // Step 6: verify correctness
    bool ok = verify_equal(data, final_output);
    std::cout << "CPU:GPU = " << cpu_percent << ":" << (100 - cpu_percent)
              << " verification: " << (ok ? "PASS" : "FAIL") << "\n";

    // Step 7: throughput
    double rows_per_sec = static_cast<double>(data.size()) / total_seconds;
    double mb_per_sec = (original_bytes / (1024.0 * 1024.0)) / total_seconds;

    Result r;
    r.ratio_label = std::to_string(cpu_percent) + ":" + std::to_string(100 - cpu_percent);
    r.throughput_mbps = mb_per_sec;
    r.throughput_rowsps = rows_per_sec;
    return r;
}

void print_table(const std::vector<Result>& results) {
    std::cout << "--------------------------------------------------------------\n";
    std::cout << "| CPU:GPU Ratio | Throughput MB/s | Throughput Rows/s       |\n";
    std::cout << "--------------------------------------------------------------\n";

    for (const auto& r : results) {
        std::cout << "| "
                  << std::setw(13) << std::left << r.ratio_label
                  << "| "
                  << std::setw(15) << std::left << std::setprecision(4) << r.throughput_mbps
                  << "| "
                  << std::setw(23) << std::left << std::setprecision(4) << r.throughput_rowsps
                  << "|\n";
    }

    std::cout << "--------------------------------------------------------------\n";
}

void save_csv(const std::vector<Result>& results) {
    std::ofstream file("ratio_results.csv");
    file << "CPU_GPU_Ratio,Throughput_MBps,Throughput_Rowsps\n";

    for (const auto& r : results) {
        file << r.ratio_label << ","
             << r.throughput_mbps << ","
             << r.throughput_rowsps << "\n";
    }

    file.close();
    std::cout << "\nResults saved to ratio_results.csv\n";
}

void save_bar_plot(const std::vector<Result>& results) {
    std::ofstream data("ratio_plot_data.dat");
    for (const auto& r : results) {
        data << r.ratio_label << " " << r.throughput_mbps << "\n";
    }
    data.close();

    std::ofstream gp("ratio_plot.gp");
    gp << "set terminal pngcairo size 1000,600\n";
    gp << "set output 'ratio_bar_plot.png'\n";
    gp << "set title 'RLE Throughput vs CPU:GPU Ratio'\n";
    gp << "set xlabel 'CPU:GPU Ratio'\n";
    gp << "set ylabel 'Throughput (MB/s)'\n";
    gp << "set style data histograms\n";
    gp << "set style fill solid border -1\n";
    gp << "set boxwidth 0.7\n";
    gp << "set grid ytics\n";
    gp << "plot 'ratio_plot_data.dat' using 2:xtic(1) title 'Throughput'\n";
    gp.close();

    int rc = std::system("gnuplot ratio_plot.gp");
    if (rc == 0) {
        std::cout << "Bar plot saved to ratio_bar_plot.png\n";
    } else {
        std::cout << "Could not generate bar plot.\n";
    }
}

int main() {
    std::size_t N = 10000000;

    // Pick ONE dataset. Start with skewed.
    std::vector<uint32_t> data = generate_skewed(N);

    std::vector<Result> results;
    results.push_back(run_ratio_benchmark(data, 100));
    results.push_back(run_ratio_benchmark(data, 75));
    results.push_back(run_ratio_benchmark(data, 50));
    results.push_back(run_ratio_benchmark(data, 25));
    results.push_back(run_ratio_benchmark(data, 0));

    print_table(results);
    save_csv(results);
    save_bar_plot(results);

    return 0;
}
