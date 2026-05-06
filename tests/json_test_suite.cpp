#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "../include/JSON.hpp"
#include "../include/JsonParser.hpp"

static constexpr auto PARSE_TIMEOUT = std::chrono::milliseconds(500);

// Runs the JSONTestSuite against the Serde JSON parser.
// File-name prefixes determine the expected outcome:
//   y_  must be accepted (parse succeeds)
//   n_  must be rejected (parse throws)
//   i_  implementation-defined (shown but not counted as pass/fail)
int main(const int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: json_test_suite <path/to/test_parsing>\n";
    return 1;
  }

  namespace fs = std::filesystem;
  const fs::path testDir = argv[1];

  // Collect and sort: y_ first, then n_, then i_
  std::vector<fs::path> entries;
  for (const auto &entry: fs::directory_iterator(testDir)) {
    if (entry.path().extension() == ".json")
      entries.push_back(entry.path());
  }
  std::sort(entries.begin(), entries.end(), [](const fs::path &a, const fs::path &b) {
    const std::string na = a.filename().string();
    const std::string nb = b.filename().string();
    const int ga = na.starts_with("y_") ? 0 : na.starts_with("n_") ? 1 : 2;
    const int gb = nb.starts_with("y_") ? 0 : nb.starts_with("n_") ? 1 : 2;
    return ga != gb ? ga < gb : na < nb;
  });

  int passed = 0, failed = 0;
  std::vector<std::string> failures;

  auto runTest = [&](const fs::path &path) -> bool {
    // Use packaged_task + detached thread: unlike std::async, the resulting
    // future does NOT block in its destructor, so timed-out threads don't stall.
    std::packaged_task<bool()> task([&path]() -> bool {
      std::ifstream file(path, std::ios::binary);
      try {
        Serde::JSONParser(file).Parse();
        return true;
      } catch (...) {
        return false;
      }
    });
    auto future = task.get_future();
    std::thread(std::move(task)).detach();
    if (future.wait_for(PARSE_TIMEOUT) != std::future_status::ready)
      return false; // timeout = rejected
    return future.get();
  };

  std::string currentSection;

  for (const auto &path: entries) {
    const std::string name = path.filename().string();
    const bool mustAccept = name.starts_with("y_");
    const bool mustReject = name.starts_with("n_");
    const bool impl       = !mustAccept && !mustReject;

    const std::string section = mustAccept ? "y_" : mustReject ? "n_" : "i_";
    if (section != currentSection) {
      if (!currentSection.empty()) std::cout << "\n";
      std::cout << "--- " << section << " tests ---\n";
      currentSection = section;
    }

    std::cout << "  " << name << " ... " << std::flush;
    const bool accepted = runTest(path);

    if (impl) {
      std::cout << (accepted ? "accepted" : "rejected") << "\n" << std::flush;
    } else if (mustAccept && !accepted) {
      std::cout << "FAIL [should accept]\n" << std::flush;
      failures.push_back("FAIL [should accept]: " + name);
      ++failed;
    } else if (mustReject && accepted) {
      std::cout << "FAIL [should reject]\n" << std::flush;
      failures.push_back("FAIL [should reject]: " + name);
      ++failed;
    } else {
      std::cout << "ok\n" << std::flush;
      ++passed;
    }
  }

  std::cout << "\n";
  for (const auto &f: failures)
    std::cout << f << "\n";

  std::cout << "\n" << passed << " passed, " << failed << " failed\n";

  return failed > 0 ? 1 : 0;
}
