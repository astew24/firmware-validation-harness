// Unit tests for the firmware pieces, driven by a scriptable fake HAL.

#include <cstdint>
#include <vector>

#include "fw/crc16.hpp"
#include "fw/packet.hpp"
#include "fw/ring_buffer.hpp"
#include "fw/sensor_controller.hpp"
#include "test_framework.hpp"

namespace {

struct FakeHal : fw::Hal {
  std::uint32_t now = 0;
  bool present = true;
  std::int16_t value = 100;
  std::vector<fw::Telemetry> packets;

  bool read_sensor(std::int16_t& out) override {
    if (!present) return false;
    out = value;
    return true;
  }
  std::uint32_t now_ms() const override { return now; }
  void transmit(const std::uint8_t* data, std::size_t len) override {
    fw::Telemetry t;
    if (fw::decode(data, len, t)) packets.push_back(t);
  }
};

// One tick per simulated millisecond.
void run(fw::SensorController& c, FakeHal& h, std::uint32_t ticks) {
  for (std::uint32_t i = 0; i < ticks; ++i) {
    c.tick();
    ++h.now;
  }
}

}  // namespace

TEST(crc16_matches_standard_check_value) {
  const char* msg = "123456789";
  CHECK_EQ(fw::crc16_ccitt(reinterpret_cast<const std::uint8_t*>(msg), 9), 0x29B1);
}

TEST(ring_buffer_keeps_newest_values) {
  fw::RingBuffer<std::int16_t, 4> rb;
  for (std::int16_t v = 1; v <= 10; ++v) rb.push(v);
  CHECK(rb.full());
  CHECK_EQ(rb.size(), 4u);
  CHECK_EQ(rb.mean(), 8);  // (7 + 8 + 9 + 10) / 4
  CHECK_EQ(rb.newest(), 10);
}

TEST(packet_round_trip) {
  fw::Telemetry t;
  t.seq = 200;
  t.state = fw::State::Fault;
  t.fault = fw::FaultCode::OutOfRange;
  t.filtered = -1234;
  t.raw = 2600;
  t.uptime_ms = 123456;
  const auto bytes = fw::encode(t);
  fw::Telemetry d;
  CHECK(fw::decode(bytes.data(), bytes.size(), d));
  CHECK_EQ(d.seq, 200);
  CHECK(d.state == fw::State::Fault);
  CHECK(d.fault == fw::FaultCode::OutOfRange);
  CHECK_EQ(d.filtered, -1234);
  CHECK_EQ(d.raw, 2600);
  CHECK_EQ(d.uptime_ms, 123456u);
}

TEST(crc_catches_every_single_bit_flip) {
  fw::Telemetry t;
  t.uptime_ms = 42;
  t.filtered = 17;
  const auto good = fw::encode(t);
  int caught = 0;
  for (std::size_t byte = 0; byte < good.size(); ++byte) {
    for (int bit = 0; bit < 8; ++bit) {
      auto bad = good;
      bad[byte] = static_cast<std::uint8_t>(bad[byte] ^ (1u << bit));
      fw::Telemetry d;
      if (!fw::decode(bad.data(), bad.size(), d)) ++caught;
    }
  }
  CHECK_EQ(caught, 14 * 8);
}

TEST(controller_calibrates_then_runs) {
  FakeHal h;
  fw::SensorController c(h);
  run(c, h, 1);
  CHECK(c.state() == fw::State::Calibrate);
  run(c, h, 64);
  CHECK(c.state() == fw::State::Run);
  CHECK_EQ(c.offset(), 100);
}

TEST(controller_flags_sensor_timeout_on_time) {
  FakeHal h;
  fw::SensorController c(h);
  run(c, h, 200);
  CHECK(c.state() == fw::State::Run);
  h.present = false;  // last sample was at t=199
  run(c, h, 19);      // t=200..218
  CHECK(c.state() == fw::State::Run);
  run(c, h, 1);       // t=219: 20 ms without a sample
  CHECK(c.state() == fw::State::Fault);
  CHECK(c.fault() == fw::FaultCode::SensorTimeout);
}

TEST(controller_recovers_after_good_streak) {
  FakeHal h;
  fw::SensorController c(h);
  run(c, h, 200);
  h.present = false;
  run(c, h, 30);
  CHECK(c.state() == fw::State::Fault);
  // The signal moved a lot during the gap. That must not count as a spike,
  // because the jump reference is reset after a timeout.
  h.present = true;
  h.value = 600;
  run(c, h, 49);
  CHECK(c.state() == fw::State::Fault);
  run(c, h, 1);
  CHECK(c.state() == fw::State::Run);
  CHECK(c.fault() == fw::FaultCode::None);
}

TEST(controller_flags_out_of_range) {
  FakeHal h;
  fw::SensorController c(h);
  run(c, h, 200);
  h.value = 2600;
  run(c, h, 1);
  CHECK(c.state() == fw::State::Fault);
  CHECK(c.fault() == fw::FaultCode::OutOfRange);
}

TEST(controller_flags_implausible_jump) {
  FakeHal h;
  fw::SensorController c(h);
  run(c, h, 200);
  h.value = 100 + 401;
  run(c, h, 1);
  CHECK(c.fault() == fw::FaultCode::ImplausibleJump);
}

TEST(controller_accepts_change_at_the_limit) {
  FakeHal h;
  fw::SensorController c(h);
  run(c, h, 200);
  h.value = 100 + 400;
  run(c, h, 1);
  CHECK(c.state() == fw::State::Run);
}

TEST(controller_filters_with_moving_average) {
  FakeHal h;
  fw::SensorController c(h);
  run(c, h, 100);
  h.value = 180;
  run(c, h, 8);
  CHECK_EQ(c.filtered(), 80);  // bias of 100 removed, window full of 180s
}

TEST(telemetry_at_100hz_plus_state_changes) {
  FakeHal h;
  fw::SensorController c(h);
  run(c, h, 1000);
  // ~100 periodic packets plus the BOOT->CALIBRATE and CALIBRATE->RUN events.
  CHECK(h.packets.size() >= 100 && h.packets.size() <= 103);
  CHECK(h.packets.front().state == fw::State::Calibrate);
}
