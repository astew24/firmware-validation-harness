#pragma once

#include <cstdint>
#include <vector>

#include "fw/sensor_controller.hpp"
#include "harness/scenario.hpp"

namespace harness {

// Small deterministic RNG so every run is reproducible from its seed.
class XorShift32 {
 public:
  explicit XorShift32(std::uint32_t seed) : s_(seed ? seed : 0x9E3779B9u) {}
  std::uint32_t next() {
    s_ ^= s_ << 13;
    s_ ^= s_ >> 17;
    s_ ^= s_ << 5;
    return s_;
  }
  double uniform() { return next() / 4294967296.0; }

 private:
  std::uint32_t s_;
};

// Simulated hardware: feeds the firmware a noisy sensor signal with faults
// injected per the scenario, runs a fake clock, and captures every byte the
// firmware sends (optionally flipping bits on the wire).
class SimHal : public fw::Hal {
 public:
  SimHal(const Scenario& sc, std::uint32_t seed);

  void set_time(std::uint32_t t) { now_ = t; }

  bool read_sensor(std::int16_t& out) override;
  std::uint32_t now_ms() const override { return now_; }
  void transmit(const std::uint8_t* data, std::size_t len) override;

  const std::vector<std::uint8_t>& wire() const { return wire_; }
  const std::vector<fw::PacketBytes>& sent() const { return sent_; }
  const std::vector<bool>& corrupted() const { return corrupted_; }

 private:
  const Scenario& sc_;
  XorShift32 noise_rng_;
  XorShift32 wire_rng_;
  std::uint32_t now_ = 0;
  std::vector<std::uint8_t> wire_;
  std::vector<fw::PacketBytes> sent_;
  std::vector<bool> corrupted_;
};

}  // namespace harness
