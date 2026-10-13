// Компілятор: MSVC (Visual Studio 2022, v143), x64, конфігурація Release, стандарт C++20
#include <algorithm>
#include <execution>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include "utils.h"

constexpr int DEFAULT_RUNS = 5;
constexpr int SLOW_RUNS = 2;
constexpr size_t LARGE_SIZE = 1'000'000;
constexpr int FALLBACK_THREADS = 4;
constexpr int MAX_K_MULTIPLIER = 32; 
constexpr int GOOD_ENOUGH_PERCENT = 10;

constexpr int NAME_WIDTH = 20;
constexpr int TIME_WIDTH = 15;
constexpr int SPEEDUP_WIDTH = 15;
constexpr int LINE_WIDTH = NAME_WIDTH + TIME_WIDTH + SPEEDUP_WIDTH + 6;

void print_line() {
    std::cout << std::string(LINE_WIDTH, '-') << "\n";
}

void print_table_header(const std::string& first_column) {
    std::cout << std::left << std::setw(NAME_WIDTH) << first_column
        << std::setw(TIME_WIDTH) << "Time (ms)"
        << std::setw(SPEEDUP_WIDTH) << "Speedup"
        << "Check\n";
    print_line();
}

void print_row(const std::string& name, const Result& result, double base_time) {
    std::cout << std::left << std::setw(NAME_WIDTH) << name
        << std::setw(TIME_WIDTH) << result.time_ms
        << std::setw(SPEEDUP_WIDTH) << base_time / result.time_ms
        << (result.correct ? "OK" : "ERROR") << "\n";
}

std::vector<int> make_k_list(int hw) {
    std::vector<int> ks;
    for (int k = 1; k < hw; k *= 2) {
        ks.push_back(k);
    }
    for (int mult = 1; mult <= MAX_K_MULTIPLIER; mult *= 2) {
        ks.push_back(hw * mult);
    }
    return ks;
}

template<typename Op>
class Experiment {
public:
    Experiment(const std::string& name, const std::vector<double>& data,
        Op op, Settings settings, int hw)
        : name_(name), data_(data), op_(op), settings_(settings), hw_(hw),
        ref_(data.size()), test_(data.size()), base_time_(0.0) {
    }

    void run() {
        print_conditions();
        std::transform(data_.begin(), data_.end(), ref_.begin(), op_);
        run_policies();
        run_custom();
    }

private:
    std::string name_;
    const std::vector<double>& data_;
    Op op_;
    Settings settings_;
    int hw_;
    std::vector<double> ref_;
    std::vector<double> test_;
    double base_time_;

    void print_conditions() const {
        std::cout << "\n--- Operation: " << name_ << " ---\n";
        std::cout << "Conditions: minimum of " << settings_.runs << " runs, warm-up run: "
            << (settings_.warm_up ? "yes" : "no")
            << ", thread creation time is included.\n";
        std::cout << "Speedup = time of 'No policy' / time of the row.\n\n";
    }

    template<typename Func>
    Result measure(Func routine) {
        std::fill(test_.begin(), test_.end(), 0.0);
        Result result{};
        result.time_ms = measure_time(routine, settings_);
        result.correct = verify_result(ref_, test_);
        return result;
    }

    template<typename Policy>
    void run_policy(const std::string& name, Policy policy) {
        Result result = measure([&]() {
            std::transform(policy, data_.begin(), data_.end(), test_.begin(), op_);
            });
        print_row(name, result, base_time_);
    }

    void run_policies() {
        print_table_header("Algorithm/Policy");

        Result base = measure([&]() {
            std::transform(data_.begin(), data_.end(), test_.begin(), op_);
            });
        base_time_ = base.time_ms;
        print_row("1. No policy", base, base_time_);

        run_policy("2. seq", std::execution::seq);
        run_policy("2. unseq", std::execution::unseq);
        run_policy("2. par", std::execution::par);
        run_policy("2. par_unseq", std::execution::par_unseq);
        std::cout << "\n";
    }

    void run_custom() {
        std::cout << "3. Custom algorithm (K threads):\n";
        print_table_header("Threads (K)");

        std::vector<int> ks = make_k_list(hw_);
        std::vector<double> times;

        for (int k : ks) {
            Result result = measure([&]() { custom_transform(data_, test_, op_, k); });
            print_row("K = " + std::to_string(k), result, base_time_);
            times.push_back(result.correct ? result.time_ms : VERY_LARGE_TIME);
        }

        print_line();
        print_best_k(ks, times);
    }

    void print_best_k(const std::vector<int>& ks, const std::vector<double>& times) const {
        size_t best_index = 0;
        for (size_t i = 1; i < ks.size(); ++i) {
            if (times[i] < times[best_index]) best_index = i;
        }

        double limit = times[best_index] * (1.0 + GOOD_ENOUGH_PERCENT / 100.0);
        size_t good_index = best_index;
        for (size_t i = 0; i < ks.size(); ++i) {
            if (times[i] <= limit) {
                good_index = i;
                break;
            }
        }

        std::cout << "Best K: " << ks[best_index] << "\n";
        std::cout << "Smallest K within " << GOOD_ENOUGH_PERCENT << "% of the best time: "
            << ks[good_index] << "\n";
        std::cout << "Hardware threads (HW): " << hw_ << "\n";
        std::cout << "Ratio (Best K / HW): " << static_cast<double>(ks[best_index]) / hw_ << "\n";
        std::cout << "Ratio (that K / HW): " << static_cast<double>(ks[good_index]) / hw_ << "\n";
    }
};

void print_build_mode() {
#ifndef NDEBUG
    std::cout << "WARNING: NDEBUG is not defined, this is not a Release build!\n";
#endif
#if defined(__OPTIMIZE__) || defined(OPTIMIZATION_ON)
    std::cout << "Build mode: Release, optimization ON\n";
#else
    std::cout << "Build mode: Release, optimization OFF\n";
#endif
}

int get_hardware_threads() {
    int hw = std::thread::hardware_concurrency();
    if (hw == 0) hw = FALLBACK_THREADS;
    return hw;
}

Settings choose_settings(size_t size, bool is_slow_op) {
    if (is_slow_op && size >= LARGE_SIZE) {
        return { SLOW_RUNS, false };
    }
    return { DEFAULT_RUNS, true };
}

void run_all_experiments(int hw) {
    const std::vector<std::string> files = {
        "data_10k.txt", "data_100k.txt", "data_1m.txt", "data_10m.txt"
    };

    for (const auto& filename : files) {
        std::cout << "\n========================================\n";
        std::cout << "File: " << filename << "\n";

        std::vector<double> data = load_data(filename);
        if (data.empty()) {
            std::cerr << "Error reading " << filename << ". Skipping.\n";
            continue;
        }
        std::cout << "Sample size: " << data.size() << " elements\n";

        Experiment<FastOp> fast("FastOp", data, FastOp{}, choose_settings(data.size(), false), hw);
        fast.run();

        Experiment<SlowOp> slow("SlowOp", data, SlowOp{}, choose_settings(data.size(), true), hw);
        slow.run();
    }
}

int main() {
    std::cout << std::fixed << std::setprecision(4);
    print_build_mode();
    int hw = get_hardware_threads();
    std::cout << "Hardware threads (HW): " << hw << "\n";
    run_all_experiments(hw);
    return 0;
}