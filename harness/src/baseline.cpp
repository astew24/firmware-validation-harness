#include "harness/baseline.hpp"

#include <fstream>

namespace harness {
namespace {

bool ends_with(const std::string& s, const std::string& suffix) {
  return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool is_check(const std::string& key) {
  return ends_with(key, "/passed") || key.find("/check/") != std::string::npos;
}

}  // namespace

MetricMap flatten(const std::vector<ScenarioResult>& results) {
  MetricMap m;
  for (const auto& r : results) {
    const std::string p = r.name + "/";
    m[p + "passed"] = r.passed() ? 1 : 0;
    for (const auto& c : r.checks) m[p + "check/" + c.name] = c.passed ? 1 : 0;
    m[p + "max_detection_latency_ms"] = r.max_detection_latency_ms;
    m[p + "max_recovery_ms"] = r.max_recovery_ms;
    m[p + "false_alarms"] = r.false_alarms;
    m[p + "max_tracking_error"] = r.max_tracking_error;
    m[p + "packets_accepted"] = r.packets_accepted;
  }
  return m;
}

bool write_metrics(const std::string& path, const MetricMap& m) {
  std::ofstream f(path);
  if (!f) return false;
  f << "key,value\n";
  for (const auto& [key, value] : m) f << key << "," << value << "\n";
  return static_cast<bool>(f);
}

bool read_metrics(const std::string& path, MetricMap& m) {
  std::ifstream f(path);
  if (!f) return false;
  std::string line;
  std::getline(f, line);  // header
  while (std::getline(f, line)) {
    const auto comma = line.rfind(',');
    if (comma == std::string::npos) continue;
    m[line.substr(0, comma)] = std::stod(line.substr(comma + 1));
  }
  return true;
}

std::vector<Regression> compare(const MetricMap& baseline, const MetricMap& current) {
  std::vector<Regression> out;
  for (const auto& [key, b] : baseline) {
    const auto it = current.find(key);
    if (it == current.end()) {
      if (is_check(key) && b == 1) out.push_back({key, b, 0, "check missing from this run"});
      continue;
    }
    const double c = it->second;
    if (is_check(key)) {
      if (b == 1 && c == 0) out.push_back({key, b, c, "passed in baseline, fails now"});
    } else if (ends_with(key, "/max_detection_latency_ms")) {
      if (c > b + 5) out.push_back({key, b, c, "fault detection got slower"});
    } else if (ends_with(key, "/max_recovery_ms")) {
      if (c > b + 20) out.push_back({key, b, c, "recovery got slower"});
    } else if (ends_with(key, "/false_alarms")) {
      if (c > b) out.push_back({key, b, c, "new false alarms"});
    } else if (ends_with(key, "/max_tracking_error")) {
      if (c > b * 1.25 + 2) out.push_back({key, b, c, "tracking error grew"});
    }
  }
  return out;
}

}  // namespace harness
