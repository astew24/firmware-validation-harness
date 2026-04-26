#include "harness/sim_hal.hpp"

#include <algorithm>

namespace harness {

SimHal::SimHal(const Scenario& sc, std::uint32_t seed)
    : sc_(sc), noise_rng_(seed * 2654435761u + 1u), wire_rng_(seed ^ 0xA5A5A5A5u) {}

bool SimHal::read_sensor(std::int16_t& out) {
  for (const auto& f : sc_.faults) {
    if (f.type == Injection::SensorDropout && now_ >= f.start_ms && now_ < f.end_ms) return false;
  }
  int v = kSensorBias + true_signal(now_) + static_cast<int>(noise_rng_.next() % 21) - 10;
  for (const auto& f : sc_.faults) {
    if (now_ < f.start_ms || now_ >= f.end_ms) continue;
    if (f.type == Injection::NoiseSpike) v += 900;
    if (f.type == Injection::OutOfRange) v = 2600;
  }
  out = static_cast<std::int16_t>(v);
  return true;
}

void SimHal::transmit(const std::uint8_t* data, std::size_t len) {
  fw::PacketBytes pkt{};
  std::copy(data, data + std::min(len, pkt.size()), pkt.begin());
  sent_.push_back(pkt);

  const bool corrupt =
      sc_.wire_corruption_rate > 0.0 && wire_rng_.uniform() < sc_.wire_corruption_rate;
  corrupted_.push_back(corrupt);

  fw::PacketBytes on_wire = pkt;
  if (corrupt) {
    const std::uint32_t byte = wire_rng_.next() % static_cast<std::uint32_t>(on_wire.size());
    const std::uint32_t bit = wire_rng_.next() % 8u;
    on_wire[byte] = static_cast<std::uint8_t>(on_wire[byte] ^ (1u << bit));
  }
  wire_.insert(wire_.end(), on_wire.begin(), on_wire.end());
}

}  // namespace harness
