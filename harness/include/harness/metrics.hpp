#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "harness/validator.hpp"

namespace harness {

// Collects results across validation cycles and renders them in the
// Prometheus text exposition format for scraping.
class MetricsRegistry {
 public:
  void record_cycle(const std::vector<ScenarioResult>& results, bool bad_build, double unix_time);
  std::string render() const;

 private:
  struct Series {
    std::uint64_t runs = 0;
    std::uint64_t check_failures = 0;
    std::uint64_t packets_accepted = 0;
    std::uint64_t packets_corrupted = 0;
    std::uint64_t frames_rejected = 0;
    std::uint64_t false_alarms = 0;
    double passed = 0;
    double detection_latency_ms = -1;
    double recovery_ms = -1;
    double tracking_error = 0;
  };

  mutable std::mutex mu_;
  std::map<std::string, Series> series_;
  std::uint64_t cycles_ = 0;
  bool bad_build_ = false;
  double last_cycle_time_ = 0;
};

}  // namespace harness
