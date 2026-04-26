#include "harness/scenario.hpp"

#include <cmath>

namespace harness {

std::int16_t true_signal(std::uint32_t t_ms) {
  if (t_ms < 100) return 0;  // at rest while the firmware calibrates
  const double kPi = 3.14159265358979323846;
  const double phase = 2.0 * kPi * static_cast<double>(t_ms - 100) / 1000.0;
  return static_cast<std::int16_t>(std::lround(1000.0 * std::sin(phase)));
}

fw::FaultCode expected_fault(Injection i) {
  switch (i) {
    case Injection::SensorDropout: return fw::FaultCode::SensorTimeout;
    case Injection::NoiseSpike: return fw::FaultCode::ImplausibleJump;
    case Injection::OutOfRange: return fw::FaultCode::OutOfRange;
  }
  return fw::FaultCode::None;
}

const char* to_string(Injection i) {
  switch (i) {
    case Injection::SensorDropout: return "sensor_dropout";
    case Injection::NoiseSpike: return "noise_spike";
    case Injection::OutOfRange: return "out_of_range";
  }
  return "?";
}

std::vector<Scenario> builtin_scenarios() {
  using I = Injection;
  return {
      {"nominal", 3000, {}, 0.0},
      {"sensor_dropout", 3000, {{I::SensorDropout, 1000, 1080}, {I::SensorDropout, 2000, 2150}}, 0.0},
      {"noise_spike", 3000,
       {{I::NoiseSpike, 1200, 1201}, {I::NoiseSpike, 1800, 1801}, {I::NoiseSpike, 2650, 2651}}, 0.0},
      {"out_of_range", 3000, {{I::OutOfRange, 1500, 1600}}, 0.0},
      {"wire_corruption", 3000, {}, 0.05},
      {"combined", 3000,
       {{I::SensorDropout, 800, 860}, {I::NoiseSpike, 1500, 1501}, {I::OutOfRange, 2200, 2260}}, 0.02},
  };
}

}  // namespace harness
