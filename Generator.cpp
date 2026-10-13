#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>

constexpr double MIN_VALUE = 1.0;
constexpr double MAX_VALUE = 100.0;

void generate_file(const std::string& filename, size_t size, unsigned int seed) {
    std::ofstream out(filename);
    if (!out) {
        std::cerr << "Error: could not create file " << filename << "\n";
        return;
    }

    std::mt19937 gen(seed);
    std::uniform_real_distribution<double> dist(MIN_VALUE, MAX_VALUE);

    out << size << "\n";
    out << std::fixed << std::setprecision(6);
    for (size_t i = 0; i < size; ++i) {
        out << dist(gen) << "\n";
    }
    std::cout << "Generated: " << filename << " (" << size << " elements)\n";
}

int main() {
    generate_file("data_10k.txt", 10'000, 42);
    generate_file("data_100k.txt", 100'000, 43);
    generate_file("data_1m.txt", 1'000'000, 44);
    generate_file("data_10m.txt", 10'000'000, 45);
    return 0;
}