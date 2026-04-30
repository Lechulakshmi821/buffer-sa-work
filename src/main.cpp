#include <iostream>
#include <vector>
#include <cstdint>
#include <chrono>
#include <iomanip>
#include <string>
#include <fstream>
#include <cstdlib>

#include "rle.hpp"

std::vector<uint32_t> generate_skewed(size_t N);

const int NUM_RUNS = 10;
const int WARMUP_RUNS = 2;

struct RunData {
    std::string ratio_label;
    int run_number;
    double cpu_time;
    double gpu_time;
    double total_time;
    double throughput_mbps;
};

struct Result {
    std::string ratio_label;
    double avg_mbps;
    double min_mbps;
    double max_mbps;
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

Result run_ratio_benchmark(const std::vector<uint32_t>& data,
                           int cpu_percent,
                           std::vector<RunData>& all_runs) {
    std::vector<RLEPair> compressed = rle_compress(data);

    std::size_t total_pairs = compressed.size();
    std::size_t cpu_pairs = (total_pairs * cpu_percent) / 100;

    std::vector<RLEPair> cpu_part(compressed.begin(), compressed.begin() + cpu_pairs);
    std::vector<RLEPair> gpu_part(compressed.begin() + cpu_pairs, compressed.end());

    std::string ratio_label =
        std::to_string(cpu_percent) + ":" + std::to_string(100 - cpu_percent);

    double original_bytes = static_cast<double>(data.size() * sizeof(uint32_t));

    std::cout << "\nCPU:GPU ratio = " << ratio_label << "\n";
    std::cout << "CPU pairs = " << cpu_part.size()
              << ", GPU pairs = " << gpu_part.size() << "\n";

    // GPU warm-up before measured runs
    if (!gpu_part.empty()) {
        double warmup_gpu_time = 0.0;
        auto warmup_out = rle_decompress_gpu(gpu_part, warmup_gpu_time);
        std::cout << "GPU warm-up done. Time = " << warmup_gpu_time << " s\n";
    }

    std::vector<double> stable_mbps;

    for (int run = 1; run <= NUM_RUNS; run++) {
        std::vector<uint32_t> cpu_output;
        std::vector<uint32_t> gpu_output;

        auto total_start = std::chrono::high_resolution_clock::now();

        auto cpu_start = std::chrono::high_resolution_clock::now();
        if (!cpu_part.empty()) {
            cpu_output = rle_decompress_cpu(cpu_part);
        }
        auto cpu_end = std::chrono::high_resolution_clock::now();

        double cpu_time =
            std::chrono::duration<double>(cpu_end - cpu_start).count();

        double gpu_time = 0.0;
        if (!gpu_part.empty()) {
            gpu_output = rle_decompress_gpu(gpu_part, gpu_time);
        }

        auto final_output = concat_vectors(cpu_output, gpu_output);

        auto total_end = std::chrono::high_resolution_clock::now();
        double total_time =
            std::chrono::duration<double>(total_end - total_start).count();

        if (run == 1) {
            bool ok = verify_equal(data, final_output);
            std::cout << "Verification: " << (ok ? "PASS" : "FAIL") << "\n";
        }

        double mbps =
            (original_bytes / (1024.0 * 1024.0)) / total_time;

        bool is_warmup = run <= WARMUP_RUNS;

        std::cout << "Run " << run
                  << (is_warmup ? " (warm-up)" : "")
                  << " | CPU time = " << cpu_time
                  << " | GPU time = " << gpu_time
                  << " | Total time = " << total_time
                  << " | MB/s = " << mbps << "\n";

        all_runs.push_back({
            ratio_label,
            run,
            cpu_time,
            gpu_time,
            total_time,
            mbps
        });

        if (!is_warmup) {
            stable_mbps.push_back(mbps);
        }
    }

    double sum = 0.0;
    for (double v : stable_mbps) sum += v;

    double avg = sum / stable_mbps.size();
    double min_val = stable_mbps[0];
    double max_val = stable_mbps[0];

    for (double v : stable_mbps) {
        if (v < min_val) min_val = v;
        if (v > max_val) max_val = v;
    }

    std::cout << "Final average excluding warm-up = " << avg << " MB/s\n";

    return {ratio_label, avg, min_val, max_val};
}

void save_all_runs_csv(const std::vector<RunData>& all_runs) {
    std::ofstream file("ratio_results_all_runs.csv");

    file << "CPU_GPU_Ratio,Run,CPU_Time,GPU_Time,Total_Time,Throughput_MBps\n";

    for (const auto& r : all_runs) {
        file << r.ratio_label << ","
             << r.run_number << ","
             << r.cpu_time << ","
             << r.gpu_time << ","
             << r.total_time << ","
             << r.throughput_mbps << "\n";
    }

    file.close();
    std::cout << "\nSaved all runs to ratio_results_all_runs.csv\n";
}

void save_final_csv(const std::vector<Result>& results) {
    std::ofstream file("ratio_results_final.csv");

    file << "CPU_GPU_Ratio,Avg_MBps,Min_MBps,Max_MBps\n";

    for (const auto& r : results) {
        file << r.ratio_label << ","
             << r.avg_mbps << ","
             << r.min_mbps << ","
             << r.max_mbps << "\n";
    }

    file.close();
    std::cout << "Saved final results to ratio_results_final.csv\n";
}

void save_bar_plot(const std::vector<Result>& results) {
    std::ofstream data("ratio_plot_data.dat");

    for (std::size_t i = 0; i < results.size(); i++) {
        double lower_err = results[i].avg_mbps - results[i].min_mbps;
        double upper_err = results[i].max_mbps - results[i].avg_mbps;

        data << i << " "
             << results[i].avg_mbps << " "
             << lower_err << " "
             << upper_err << " "
             << results[i].ratio_label << "\n";
    }

    data.close();

    std::ofstream gp("ratio_plot.gp");

    gp << "set terminal pngcairo size 1000,600\n";
    gp << "set output 'ratio_bar_plot.png'\n";
    gp << "set title 'RLE Throughput vs CPU:GPU Ratio (Warm-up Excluded)'\n";
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
        std::cout << "Saved plot to ratio_bar_plot.png\n";
    } else {
        std::cout << "Could not generate plot.\n";
    }
}

void print_final_table(const std::vector<Result>& results) {
    std::cout << "\n-----------------------------------------------------\n";
    std::cout << "| Ratio | Avg MB/s | Min MB/s | Max MB/s |\n";
    std::cout << "-----------------------------------------------------\n";

    for (const auto& r : results) {
        std::cout << "| "
                  << std::setw(7) << std::left << r.ratio_label
                  << "| "
                  << std::setw(9) << std::left << r.avg_mbps
                  << "| "
                  << std::setw(9) << std::left << r.min_mbps
                  << "| "
                  << std::setw(9) << std::left << r.max_mbps
                  << "|\n";
    }

    std::cout << "-----------------------------------------------------\n";
}

int main() {
    std::size_t N = 10000000;

    std::vector<uint32_t> data = generate_skewed(N);

    std::vector<RunData> all_runs;
    std::vector<Result> final_results;

    final_results.push_back(run_ratio_benchmark(data, 100, all_runs));
    final_results.push_back(run_ratio_benchmark(data, 75, all_runs));
    final_results.push_back(run_ratio_benchmark(data, 50, all_runs));
    final_results.push_back(run_ratio_benchmark(data, 25, all_runs));
    final_results.push_back(run_ratio_benchmark(data, 0, all_runs));

    print_final_table(final_results);

    save_all_runs_csv(all_runs);
    save_final_csv(final_results);
    save_bar_plot(final_results);

    return 0;
}
