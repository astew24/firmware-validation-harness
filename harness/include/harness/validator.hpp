#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "fw/sensor_controller.hpp"
#include "harness/scenario.hpp"

namespace harness {

struct Check {
  std::string name;
  bool passed = false;
  std::string detail;
};

// Pass/fail limits for one scenario run.
struct Limits {
  std::uint32_t boot_deadline_ms = 200;  // must reach RUN by then
  std::uint32_t max_detection_ms = 30;   // fault start -> FAULT reported
  std::uint32_t max_recovery_ms = 150;   // fault cleared -> RUN again
  std::int32_t max_tracking_error = 50;  // |filtered output - true signal|
  double min_packet_ratio = 0.9;         // accepted packets vs 100 Hz schedule
};

struct ScenarioResult {
  std::string name;
  std::vector<Check> checks;
  std::uint32_t packets_sent = 0;
  std::uint32_t packets_accepted = 0;
  std::uint32_t packets_corrupted = 0;
  std::uint32_t frames_rejected = 0;
  std::int32_t max_detection_latency_ms = -1;  // -1: scenario injects no sensor faults
  std::int32_t max_recovery_ms = -1;
  std::uint32_t false_alarms = 0;
  std::int32_t max_tracking_error = 0;

  bool passed() const;
};

// Runs the real firmware against simulated hardware for one scenario and
// checks the telemetry it produced.
ScenarioResult run_scenario(const Scenario& sc, const fw::Config& cfg, std::uint32_t seed,
                            const Limits& lim = Limits{});

// Runs every built-in scenario (or just `only`, if given).
std::vector<ScenarioResult> run_all(const fw::Config& cfg, std::uint32_t seed,
                                    const std::string& only = "");

}  // namespace harness
