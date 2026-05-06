#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <sys/resource.h>
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

static long peakRssKB() {
  struct rusage ru{};
  getrusage(RUSAGE_SELF, &ru);
#ifdef __APPLE__
  return ru.ru_maxrss / 1024;
#else
  return ru.ru_maxrss;
#endif
}

struct Stats {
  double minMs, maxMs, medianMs, p99Ms, meanMs, mbps;
  size_t iterations;
};

static Stats measure(const std::string &src, int warmup, int iters) {
  const double fileMB = static_cast<double>(src.size()) / (1024.0 * 1024.0);

  for (int i = 0; i < warmup; ++i)
    Serde::JSONParser(src).Parse();

  std::vector<double> times;
  times.reserve(static_cast<size_t>(iters));
  for (int i = 0; i < iters; ++i) {
    const auto t0 = Clock::now();
    Serde::JSONParser(src).Parse();
    const auto t1 = Clock::now();
    times.push_back(Ms(t1 - t0).count());
  }

  std::sort(times.begin(), times.end());
  const double sum = std::accumulate(times.begin(), times.end(), 0.0);
  const double mean = sum / static_cast<double>(iters);
  const double median = times[static_cast<size_t>(iters) / 2];
  const double p99 = times[static_cast<size_t>(iters * 99 / 100)];
  const double mbps = fileMB / (median / 1000.0);

  return {times.front(), times.back(), median, p99, mean, mbps, static_cast<size_t>(iters)};
}

static void printRow(const std::string &label, const Stats &s) {
  std::cout << std::left << std::setw(20) << label
            << std::right
            << std::setw(8)  << std::fixed << std::setprecision(2) << s.minMs    << " ms"
            << std::setw(9)  << std::fixed << std::setprecision(2) << s.medianMs << " ms"
            << std::setw(9)  << std::fixed << std::setprecision(2) << s.p99Ms    << " ms"
            << std::setw(9)  << std::fixed << std::setprecision(2) << s.maxMs    << " ms"
            << std::setw(10) << std::fixed << std::setprecision(1) << s.mbps     << " MB/s"
            << "\n";
}

int main(int argc, char *argv[]) {
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

  constexpr int WARMUP = 5;
  constexpr int ITERS  = 100;

  std::cout << "Loading files...\n";
  std::vector<std::string> sources;
  sources.reserve(paths.size());
  for (const auto &p : paths) {
    const std::string src = readFile(p);
    const double mb = static_cast<double>(src.size()) / (1024.0 * 1024.0);
    std::cout << "  " << p.filename().string() << " : "
              << std::fixed << std::setprecision(2) << mb << " MB\n";
    sources.push_back(src);
  }
  std::cout << "  warmup=" << WARMUP << "  iterations=" << ITERS << "\n\n";

  const long rssBefore = peakRssKB();

  std::cout << std::left << std::setw(20) << "file"
            << std::right
            << std::setw(10) << "min"
            << std::setw(11) << "median"
            << std::setw(11) << "p99"
            << std::setw(11) << "max"
            << std::setw(13) << "throughput"
            << "\n";
  std::cout << std::string(76, '-') << "\n";

  for (size_t i = 0; i < paths.size(); ++i) {
    const Stats s = measure(sources[i], WARMUP, ITERS);
    printRow(paths[i].filename().string(), s);
  }

  const long rssAfter = peakRssKB();

  std::cout << "\n";
  std::cout << "Peak RSS before : " << rssBefore << " KB\n";
  std::cout << "Peak RSS after  : " << rssAfter  << " KB\n";
  std::cout << "Delta           : " << (rssAfter - rssBefore) << " KB\n";

  return 0;
}
