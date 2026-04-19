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
    double avg_throughput_mbps;
    double avg_throughput_rowsps;
    double min_throughput_mbps;
    double max_throughput_mbps;
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

// run one CPU:GPU ratio experiment 10 times and take average
Result run_ratio_benchmark(const std::vector<uint32_t>& data, int cpu_percent) {
    const int NUM_RUNS = 10;

    std::vector<double> mbps_values;
    std::vector<double> rowsps_values;

    for (int run = 0; run < NUM_RUNS; run++) {
        // Step 1: compress on CPU
        std::vector<RLEPair> compressed = rle_compress(data);

        std::size_t total_pairs = compressed.size();
        std::size_t cpu_pairs = (total_pairs * cpu_percent) / 100;

        std::vector<RLEPair> cpu_part(compressed.begin(), compressed.begin() + cpu_pairs);
        std::vector<RLEPair> gpu_part(compressed.begin() + cpu_pairs, compressed.end());

        double original_bytes = static_cast<double>(data.size() * sizeof(uint32_t));

        // Step 2: start timing
        auto start = std::chrono::high_resolution_clock::now();

        // Step 3: CPU decompress its part
        std::vector<uint32_t> cpu_output;
        if (!cpu_part.empty()) {
            cpu_output = rle_decompress_cpu(cpu_part);
        }

        // Step 4: GPU part is transferred and decompressed on GPU
        std::vector<uint32_t> gpu_output;
        double gpu_seconds_internal = 0.0;
        if (!gpu_part.empty()) {
            gpu_output = rle_decompress_gpu(gpu_part, gpu_seconds_internal);
        }

        // Step 5: combine outputs
        std::vector<uint32_t> final_output = concat_vectors(cpu_output, gpu_output);

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> total_diff = end - start;
        double total_seconds = total_diff.count();

        // verify correctness on first run only
        if (run == 0) {
            bool ok = verify_equal(data, final_output);
            std::cout << "CPU:GPU = " << cpu_percent << ":" << (100 - cpu_percent)
                      << " verification: " << (ok ? "PASS" : "FAIL") << "\n";
        }

        double rows_per_sec = static_cast<double>(data.size()) / total_seconds;
        double mb_per_sec = (original_bytes / (1024.0 * 1024.0)) / total_seconds;

        mbps_values.push_back(mb_per_sec);
        rowsps_values.push_back(rows_per_sec);
    }

    // compute average
    double sum_mbps = 0.0;
    double sum_rowsps = 0.0;
    for (int i = 0; i < NUM_RUNS; i++) {
        sum_mbps += mbps_values[i];
        sum_rowsps += rowsps_values[i];
    }

    double avg_mbps = sum_mbps / NUM_RUNS;
    double avg_rowsps = sum_rowsps / NUM_RUNS;

    // compute min and max
    double min_mbps = mbps_values[0];
    double max_mbps = mbps_values[0];

    for (double v : mbps_values) {
        if (v < min_mbps) min_mbps = v;
        if (v > max_mbps) max_mbps = v;
    }

    std::cout << "CPU:GPU = " << cpu_percent << ":" << (100 - cpu_percent)
              << " average MB/s = " << avg_mbps
              << " (min = " << min_mbps
              << ", max = " << max_mbps << ")\n";

    Result r;
    r.ratio_label = std::to_string(cpu_percent) + ":" + std::to_string(100 - cpu_percent);
    r.avg_throughput_mbps = avg_mbps;
    r.avg_throughput_rowsps = avg_rowsps;
    r.min_throughput_mbps = min_mbps;
    r.max_throughput_mbps = max_mbps;

    return r;
}

void print_table(const std::vector<Result>& results) {
    std::cout << "-----------------------------------------------------------------------------------------------\n";
    std::cout << "| CPU:GPU Ratio | Avg MB/s | Avg Rows/s           | Min MB/s | Max MB/s |\n";
    std::cout << "-----------------------------------------------------------------------------------------------\n";

    for (const auto& r : results) {
        std::cout << "| "
                  << std::setw(13) << std::left << r.ratio_label
                  << "| "
                  << std::setw(9) << std::left << std::setprecision(4) << r.avg_throughput_mbps
                  << "| "
                  << std::setw(21) << std::left << std::setprecision(4) << r.avg_throughput_rowsps
                  << "| "
                  << std::setw(9) << std::left << std::setprecision(4) << r.min_throughput_mbps
                  << "| "
                  << std::setw(9) << std::left << std::setprecision(4) << r.max_throughput_mbps
                  << "|\n";
    }

    std::cout << "-----------------------------------------------------------------------------------------------\n";
}

void save_csv(const std::vector<Result>& results) {
    std::ofstream file("ratio_results.csv");
    file << "CPU_GPU_Ratio,Avg_Throughput_MBps,Avg_Throughput_Rowsps,Min_Throughput_MBps,Max_Throughput_MBps\n";

    for (const auto& r : results) {
        file << r.ratio_label << ","
             << r.avg_throughput_mbps << ","
             << r.avg_throughput_rowsps << ","
             << r.min_throughput_mbps << ","
             << r.max_throughput_mbps << "\n";
    }

    file.close();
    std::cout << "\nResults saved to ratio_results.csv\n";
}

void save_bar_plot(const std::vector<Result>& results) {
    std::ofstream data("ratio_plot_data.dat");
    // index  avg  low  high  label
    for (std::size_t i = 0; i < results.size(); i++) {
        double lower_err = results[i].avg_throughput_mbps - results[i].min_throughput_mbps;
        double upper_err = results[i].max_throughput_mbps - results[i].avg_throughput_mbps;

        data << i << " "
             << results[i].avg_throughput_mbps << " "
             << lower_err << " "
             << upper_err << " "
             << results[i].ratio_label << "\n";
    }
    data.close();

    std::ofstream gp("ratio_plot.gp");
    gp << "set terminal pngcairo size 1000,600\n";
    gp << "set output 'ratio_bar_plot.png'\n";
    gp << "set title 'RLE Throughput vs CPU:GPU Ratio (Average of 10 Runs)'\n";
    gp << "set xlabel 'CPU:GPU Ratio'\n";
    gp << "set ylabel 'Throughput (MB/s)'\n";
    gp << "set grid ytics\n";
    gp << "set boxwidth 0.6\n";
    gp << "set style fill solid border -1\n";
    gp << "set yrange [0:*]\n";
    gp << "set xtics rotate by -45\n";
    gp << "set xtics (";
    for (std::size_t i = 0; i < results.size(); i++) {
        gp << "'" << results[i].ratio_label << "' " << i;
        if (i + 1 < results.size()) gp << ", ";
    }
    gp << ")\n";
    gp << "plot 'ratio_plot_data.dat' using 1:2 with boxes title 'Average Throughput', \\\n";
    gp << "     '' using 1:2:3:4 with yerrorbars notitle\n";
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

    // Use one dataset only, as discussed with professor/mentor
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
