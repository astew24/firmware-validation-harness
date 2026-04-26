#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "fw/packet.hpp"

namespace harness {

// Faults the harness can inject into the sensor feed.
enum class Injection { SensorDropout, NoiseSpike, OutOfRange };

struct FaultWindow {
  Injection type;
  std::uint32_t start_ms;  // inclusive
  std::uint32_t end_ms;    // exclusive
};

struct Scenario {
  std::string name;
  std::uint32_t duration_ms = 3000;
  std::vector<FaultWindow> faults;
  double wire_corruption_rate = 0.0;  // share of telemetry packets with one flipped bit
};

std::vector<Scenario> builtin_scenarios();

// The fault code the firmware should report for each injected fault.
fw::FaultCode expected_fault(Injection i);
const char* to_string(Injection i);

// Ground truth the sensor measures, before bias and noise: flat while the
// firmware calibrates, then a 1 Hz sine wave with amplitude 1000.
std::int16_t true_signal(std::uint32_t t_ms);
constexpr int kSensorBias = 120;

}  // namespace harness
