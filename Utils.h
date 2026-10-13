#pragma once
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

constexpr int SLOW_ITERATIONS = 50;

struct FastOp {
    double operator()(double x) const {
        return x * 1.5 + 2.0;
    }
};

struct SlowOp {
    double operator()(double x) const {
        double res = x;
        for (int i = 0; i < SLOW_ITERATIONS; ++i) {
            res = std::sin(res) * std::cos(res) + 1.0;
        }
        return res;
    }
};

inline std::vector<double> load_data(const std::string& filename) {
    std::ifstream in(filename);
    size_t size = 0;
    if (in >> size) {
        std::vector<double> data(size);
        for (size_t i = 0; i < size; ++i) {
            in >> data[i];
        }
        return data;
    }
    return {};
}

template<typename Func>
void custom_transform(const std::vector<double>& in, std::vector<double>& out, Func f, int K) {
    size_t size = in.size();
    if (size == 0) return;
    if (K <= 0) K = 1;
    if (static_cast<size_t>(K) > size) K = static_cast<int>(size);

    size_t chunk = size / K;
    std::vector<std::thread> threads;
    threads.reserve(K);

    for (int i = 0; i < K; ++i) {
        size_t start = i * chunk;
        size_t end = (i == K - 1) ? size : start + chunk;
        threads.emplace_back([&in, &out, f, start, end]() {
            std::transform(in.begin() + start, in.begin() + end, out.begin() + start, f);
            });
    }

    for (auto& t : threads) {
        t.join();
    }
}

constexpr double EPSILON = 1e-5;
constexpr double VERY_LARGE_TIME = 1e18;

struct Settings {
    int runs;
    bool warm_up;
};

struct Result {
    double time_ms;
    bool correct;
};

inline bool verify_result(const std::vector<double>& ref, const std::vector<double>& test) {
    if (ref.size() != test.size()) return false;
    for (size_t i = 0; i < ref.size(); ++i) {
        if (!(std::abs(ref[i] - test[i]) <= EPSILON)) return false;
    }
    return true;
}

template<typename Func>
double measure_time(Func routine, const Settings& settings) {
    if (settings.warm_up) routine();

    double best = VERY_LARGE_TIME;
    for (int i = 0; i < settings.runs; ++i) {
        auto start = std::chrono::steady_clock::now();
        routine();
        auto finish = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(finish - start).count();
        best = std::min(best, ms);
    }
    return best;
}