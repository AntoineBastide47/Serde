#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

#include "../include/JSON.hpp"
#include "../include/JsonParser.hpp"

namespace fs = std::filesystem;
using Clock = std::chrono::high_resolution_clock;
using Ms = std::chrono::duration<double, std::milli>;

static std::string readFile(const fs::path &path) {
  std::ifstream f(path, std::ios::binary | std::ios::ate);
  const std::streamsize size = f.tellg();
  f.seekg(0);
  std::string buf(static_cast<size_t>(size), '\0');
  f.read(buf.data(), size);
  return buf;
}

struct Stats {
  double minMs, maxMs, medianMs, p99Ms, mbps;
};

static constexpr int WARMUP = 5;
static constexpr int ITERS = 100;

static Stats measure(const std::string &src) {
  const double fileMB = static_cast<double>(src.size()) / (1024.0 * 1024.0);

  for (int i = 0; i < WARMUP; ++i)
    Serde::JSONParser(src).Parse();

  std::vector<double> times;
  times.reserve(ITERS);
  for (int i = 0; i < ITERS; ++i) {
    const auto t0 = Clock::now();
    Serde::JSONParser(src).Parse();
    const auto t1 = Clock::now();
    times.push_back(Ms(t1 - t0).count());
  }

  std::ranges::sort(times);
  const double median = times[static_cast<size_t>(ITERS) / 2];
  const double p99 = times[static_cast<size_t>(ITERS * 99 / 100)];
  const double mbps = fileMB / (median / 1000.0);

  return {times.front(), times.back(), median, p99, mbps};
}

static void printRow(const std::string &label, const Stats &s, const int labelWidth) {
  std::cout << std::left << std::setw(labelWidth) << label
      << std::right
      << std::setw(8) << std::fixed << std::setprecision(2) << s.minMs << " ms"
      << std::setw(9) << std::fixed << std::setprecision(2) << s.medianMs << " ms"
      << std::setw(9) << std::fixed << std::setprecision(2) << s.p99Ms << " ms"
      << std::setw(9) << std::fixed << std::setprecision(2) << s.maxMs << " ms"
      << std::setw(10) << std::fixed << std::setprecision(1) << s.mbps << " MB/s"
      << "\n";
}

int main(const int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: benchmark <file.json> [file2.json ...]\n";
    return 1;
  }

  std::vector<fs::path> paths;
  for (int i = 1; i < argc; ++i) {
    fs::path p = argv[i];
    if (!fs::exists(p)) {
      std::cerr << "File not found: " << p << "\n";
      return 1;
    }
    paths.push_back(std::move(p));
  }

  std::cout << "Loading files...\n";
  std::vector<std::string> sources;
  sources.reserve(paths.size());
  for (const auto &p: paths) {
    const std::string src = readFile(p);
    const double mb = static_cast<double>(src.size()) / (1024.0 * 1024.0);
    std::cout << "  " << p.filename().string() << " : "
        << std::fixed << std::setprecision(2) << mb << " MB\n";
    sources.push_back(src);
  }
  std::cout << "  warmup=" << WARMUP << "  iterations=" << ITERS << "\n\n";

  size_t maxLabel = std::string_view("file").size();
  for (const auto &path: paths)
    maxLabel = std::max(maxLabel, path.filename().string().size());
  const int labelWidth = static_cast<int>(maxLabel + 2);

  std::cout << std::left << std::setw(labelWidth) << "file"
      << std::right
      << std::setw(10) << "min"
      << std::setw(11) << "median"
      << std::setw(11) << "p99"
      << std::setw(11) << "max"
      << std::setw(13) << "throughput"
      << "\n";
  std::cout << std::string(static_cast<size_t>(labelWidth) + 56, '-') << "\n";

  for (size_t i = 0; i < paths.size(); ++i) {
    const Stats s = measure(sources[i]);
    printRow(paths[i].filename().string(), s, labelWidth);
  }

  return 0;
}
