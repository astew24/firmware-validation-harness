#include "harness/metrics.hpp"

#include <sstream>

namespace harness {

void MetricsRegistry::record_cycle(const std::vector<ScenarioResult>& results, bool bad_build,
                                   double unix_time) {
  std::lock_guard<std::mutex> lock(mu_);
  for (const auto& r : results) {
    Series& s = series_[r.name];
    ++s.runs;
    for (const auto& c : r.checks) {
      if (!c.passed) ++s.check_failures;
    }
    s.packets_accepted += r.packets_accepted;
    s.packets_corrupted += r.packets_corrupted;
    s.frames_rejected += r.frames_rejected;
    s.false_alarms += r.false_alarms;
    s.passed = r.passed() ? 1 : 0;
    s.detection_latency_ms = r.max_detection_latency_ms;
    s.recovery_ms = r.max_recovery_ms;
    s.tracking_error = r.max_tracking_error;
  }
  ++cycles_;
  bad_build_ = bad_build;
  last_cycle_time_ = unix_time;
}

std::string MetricsRegistry::render() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::ostringstream o;

  auto family = [&](const char* name, const char* type, const char* help) {
    o << "# HELP " << name << " " << help << "\n# TYPE " << name << " " << type << "\n";
  };
  // One line per scenario. `skip_negative` hides gauges that do not apply (-1).
  auto each = [&](const char* name, auto get, bool skip_negative = false) {
    for (const auto& [scenario, s] : series_) {
      const double v = static_cast<double>(get(s));
      if (skip_negative && v < 0) continue;
      o << name << "{scenario=\"" << scenario << "\"} " << v << "\n";
    }
  };

  family("fvh_scenario_runs_total", "counter", "Scenario runs completed.");
  each("fvh_scenario_runs_total", [](const Series& s) { return s.runs; });
  family("fvh_scenario_passed", "gauge", "1 if every check passed in the last run.");
  each("fvh_scenario_passed", [](const Series& s) { return s.passed; });
  family("fvh_check_failures_total", "counter", "Failed validation checks.");
  each("fvh_check_failures_total", [](const Series& s) { return s.check_failures; });
  family("fvh_fault_detection_latency_ms", "gauge", "Slowest fault detection in the last run.");
  each("fvh_fault_detection_latency_ms", [](const Series& s) { return s.detection_latency_ms; }, true);
  family("fvh_recovery_ms", "gauge", "Slowest return to RUN after a fault cleared, last run.");
  each("fvh_recovery_ms", [](const Series& s) { return s.recovery_ms; }, true);
  family("fvh_tracking_error_max", "gauge", "Largest filtered-output error vs the true signal, last run.");
  each("fvh_tracking_error_max", [](const Series& s) { return s.tracking_error; });
  family("fvh_false_alarms_total", "counter", "Fault episodes outside injected fault windows.");
  each("fvh_false_alarms_total", [](const Series& s) { return s.false_alarms; });
  family("fvh_packets_accepted_total", "counter", "Telemetry packets that passed the CRC.");
  each("fvh_packets_accepted_total", [](const Series& s) { return s.packets_accepted; });
  family("fvh_packets_corrupted_total", "counter", "Telemetry packets corrupted on the wire by the harness.");
  each("fvh_packets_corrupted_total", [](const Series& s) { return s.packets_corrupted; });
  family("fvh_frames_rejected_total", "counter", "Frames the decoder rejected (bad CRC or fields).");
  each("fvh_frames_rejected_total", [](const Series& s) { return s.frames_rejected; });

  family("fvh_cycles_total", "counter", "Validation cycles completed.");
  o << "fvh_cycles_total " << cycles_ << "\n";
  family("fvh_bad_build", "gauge", "1 when the last cycle ran a deliberately slowed firmware build.");
  o << "fvh_bad_build " << (bad_build_ ? 1 : 0) << "\n";
  family("fvh_last_cycle_timestamp_seconds", "gauge", "Unix time of the last validation cycle.");
  o << "fvh_last_cycle_timestamp_seconds " << static_cast<long long>(last_cycle_time_) << "\n";
  return o.str();
}

}  // namespace harness
