// Tests for the harness itself: scenarios, decoder, and the regression gate.

#include <cstdint>
#include <vector>

#include "harness/baseline.hpp"
#include "harness/decoder.hpp"
#include "harness/scenario.hpp"
#include "harness/validator.hpp"
#include "test_framework.hpp"

TEST(all_builtin_scenarios_pass_across_seeds) {
  for (std::uint32_t seed = 1; seed <= 5; ++seed) {
    for (const auto& r : harness::run_all(fw::Config{}, seed)) {
      if (!r.passed()) {
        std::printf("    %s failed with seed %u\n", r.name.c_str(), static_cast<unsigned>(seed));
        for (const auto& c : r.checks) {
          if (!c.passed) std::printf("      %s: %s\n", c.name.c_str(), c.detail.c_str());
        }
      }
      CHECK(r.passed());
    }
  }
}

TEST(slow_firmware_build_fails_detection) {
  fw::Config slow;
  slow.sensor_timeout_ms = 60;
  const auto results = harness::run_all(slow, 1, "sensor_dropout");
  CHECK_EQ(results.size(), 1u);
  CHECK(!results[0].passed());
  CHECK(results[0].max_detection_latency_ms > 30);
}

TEST(baseline_compare_flags_regressions) {
  const auto good = harness::flatten(harness::run_all(fw::Config{}, 1));
  fw::Config slow;
  slow.sensor_timeout_ms = 60;
  const auto bad = harness::flatten(harness::run_all(slow, 1));
  CHECK(harness::compare(good, good).empty());
  bool saw_latency = false;
  bool saw_check = false;
  for (const auto& r : harness::compare(good, bad)) {
    if (r.key == "sensor_dropout/max_detection_latency_ms") saw_latency = true;
    if (r.key == "sensor_dropout/check/fault_detected_1") saw_check = true;
  }
  CHECK(saw_latency);
  CHECK(saw_check);
}

TEST(decoder_resyncs_and_rejects_corruption) {
  fw::Telemetry a;
  a.seq = 1;
  fw::Telemetry b;
  b.seq = 2;
  fw::Telemetry c;
  c.seq = 3;
  std::vector<std::uint8_t> wire = {0x00, fw::kSync, 0x13, 0x37};  // line noise with a fake sync byte
  auto add = [&](const fw::PacketBytes& p) { wire.insert(wire.end(), p.begin(), p.end()); };
  add(fw::encode(a));
  auto broken = fw::encode(b);
  broken[6] = static_cast<std::uint8_t>(broken[6] ^ 0x10);
  add(broken);
  add(fw::encode(c));

  const auto d = harness::decode_stream(wire);
  CHECK_EQ(d.packets.size(), 2u);
  CHECK_EQ(d.packets[0].seq, 1);
  CHECK_EQ(d.packets[1].seq, 3);
  CHECK(d.rejected_frames >= 1);
}

TEST(wire_corruption_is_caught_by_crc) {
  const auto r = harness::run_all(fw::Config{}, 7, "wire_corruption");
  CHECK_EQ(r.size(), 1u);
  CHECK(r[0].packets_corrupted > 0);
  CHECK_EQ(r[0].packets_accepted, r[0].packets_sent - r[0].packets_corrupted);
}
