#include <iostream>
#include <vector>
#include <cstdint>
#include <chrono>
#include <iomanip>
#include <string>
#include <fstream>
#include <cstdlib>

#include "rle.hpp"

// dataset generators from dataset.cpp
std::vector<uint32_t> generate_repeated(size_t N);
std::vector<uint32_t> generate_uniform(size_t N);
std::vector<uint32_t> generate_sorted(size_t N);
std::vector<uint32_t> generate_skewed(size_t N);

struct Result {
    std::string name;
    std::string compression;
    double ratio;
    double cpu_mbps;
    double cpu_rowsps;
    double gpu_mbps;
    double gpu_rowsps;
    double gpu_cpu_ratio;
};

// check if two vectors are equal
bool verify_equal(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b) {
    if (a.size() != b.size()) {
        return false;
    }

    for (std::size_t i = 0; i < a.size(); i++) {
        if (a[i] != b[i]) {
            return false;
        }
    }

    return true;
}

// run one benchmark for one distribution
Result run_benchmark(const std::string& name, const std::vector<uint32_t>& data) {
    // compress with RLE
    std::vector<RLEPair> compressed = rle_compress(data);

    double original_bytes = static_cast<double>(data.size() * sizeof(uint32_t));
    double compressed_bytes = static_cast<double>(compressed.size() * sizeof(RLEPair));
    double compression_ratio = original_bytes / compressed_bytes;

    // CPU decompression timing
    auto cpu_start = std::chrono::high_resolution_clock::now();
    std::vector<uint32_t> decompressed_cpu = rle_decompress_cpu(compressed);
    auto cpu_end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> cpu_diff = cpu_end - cpu_start;
    double cpu_seconds = cpu_diff.count();

    // GPU decompression timing
    double gpu_seconds = 0.0;
    std::vector<uint32_t> decompressed_gpu = rle_decompress_gpu(compressed, gpu_seconds);

    // verify correctness
    bool cpu_ok = verify_equal(data, decompressed_cpu);
    bool gpu_ok = verify_equal(data, decompressed_gpu);

    std::cout << name << " CPU verification: " << (cpu_ok ? "PASS" : "FAIL") << "\n";
    std::cout << name << " GPU verification: " << (gpu_ok ? "PASS" : "FAIL") << "\n";

    // throughput
    double cpu_rows_per_sec = static_cast<double>(data.size()) / cpu_seconds;
    double cpu_mb_per_sec = (original_bytes / (1024.0 * 1024.0)) / cpu_seconds;

    double gpu_rows_per_sec = static_cast<double>(data.size()) / gpu_seconds;
    double gpu_mb_per_sec = (original_bytes / (1024.0 * 1024.0)) / gpu_seconds;

    double gpu_cpu_ratio = gpu_mb_per_sec / cpu_mb_per_sec;

    Result result;
    result.name = name;
    result.compression = "RLE";
    result.ratio = compression_ratio;
    result.cpu_mbps = cpu_mb_per_sec;
    result.cpu_rowsps = cpu_rows_per_sec;
    result.gpu_mbps = gpu_mb_per_sec;
    result.gpu_rowsps = gpu_rows_per_sec;
    result.gpu_cpu_ratio = gpu_cpu_ratio;

    return result;
}

// print results in table format
void print_table(const std::vector<Result>& results) {
    std::cout << "---------------------------------------------------------------------------------------------------------\n";
    std::cout << "| Distribution | Compression | Ratio   | CPU MB/s | GPU MB/s | CPU Rows/s      | GPU Rows/s      | GPU/CPU |\n";
    std::cout << "---------------------------------------------------------------------------------------------------------\n";

    for (const auto& r : results) {
        std::cout << "| "
                  << std::setw(12) << std::left << r.name
                  << "| "
                  << std::setw(12) << std::left << r.compression
                  << "| "
                  << std::setw(8) << std::left << std::setprecision(3) << r.ratio
                  << "| "
                  << std::setw(9) << std::left << std::setprecision(4) << r.cpu_mbps
                  << "| "
                  << std::setw(9) << std::left << std::setprecision(4) << r.gpu_mbps
                  << "| "
                  << std::setw(16) << std::left << std::setprecision(4) << r.cpu_rowsps
                  << "| "
                  << std::setw(16) << std::left << std::setprecision(4) << r.gpu_rowsps
                  << "| "
                  << std::setw(8) << std::left << std::setprecision(4) << r.gpu_cpu_ratio
                  << "|\n";
    }

    std::cout << "---------------------------------------------------------------------------------------------------------\n";
}

// save results to CSV
void save_csv(const std::vector<Result>& results) {
    std::ofstream file("results.csv");

    file << "Distribution,Compression,Ratio,CPU_MBps,GPU_MBps,CPU_Rowsps,GPU_Rowsps,GPU_CPU_Ratio\n";

    for (const auto& r : results) {
        file << r.name << ","
             << r.compression << ","
             << r.ratio << ","
             << r.cpu_mbps << ","
             << r.gpu_mbps << ","
             << r.cpu_rowsps << ","
             << r.gpu_rowsps << ","
             << r.gpu_cpu_ratio << "\n";
    }

    file.close();
    std::cout << "\nResults saved to results.csv\n";
}

// create line graph using gnuplot
void save_line_graph_files(const std::vector<Result>& results) {
    // save graph data
    std::ofstream data("plot_data.dat");

    for (std::size_t i = 0; i < results.size(); i++) {
        data << i << " "
             << results[i].name << " "
             << results[i].cpu_mbps << " "
             << results[i].gpu_mbps << "\n";
    }

    data.close();

    // save gnuplot script
    std::ofstream gp("plot.gp");

    gp << "set terminal pngcairo size 1000,600\n";
    gp << "set output 'throughput_line_graph.png'\n";
    gp << "set title 'CPU vs GPU Decompression Throughput'\n";
    gp << "set xlabel 'Distribution'\n";
    gp << "set ylabel 'Throughput (MB/s)'\n";
    gp << "set grid ytics\n";
    gp << "set key left top\n";

    gp << "set xtics (";
    for (std::size_t i = 0; i < results.size(); i++) {
        gp << "'" << results[i].name << "' " << i;
        if (i + 1 < results.size()) {
            gp << ", ";
        }
    }
    gp << ")\n";

    gp << "plot 'plot_data.dat' using 1:3 with linespoints lw 2 pt 7 title 'CPU', \\\n";
    gp << "     'plot_data.dat' using 1:4 with linespoints lw 2 pt 7 title 'GPU'\n";

    gp.close();

    int rc = std::system("gnuplot plot.gp");
    if (rc != 0) {
        std::cerr << "Could not generate graph using gnuplot.\n";
    } else {
        std::cout << "Line graph saved to throughput_line_graph.png\n";
    }
}

int main() {
    std::size_t N = 10000000;

    std::vector<Result> results;

    results.push_back(run_benchmark("Repeated", generate_repeated(N)));
    results.push_back(run_benchmark("Uniform", generate_uniform(N)));
    results.push_back(run_benchmark("Sorted", generate_sorted(N)));
    results.push_back(run_benchmark("Skewed", generate_skewed(N)));

    print_table(results);
    save_csv(results);
    save_line_graph_files(results);

    return 0;
}
