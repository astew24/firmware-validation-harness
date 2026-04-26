#include "harness/validator.hpp"

#include <algorithm>
#include <cstdlib>

#include "harness/decoder.hpp"
#include "harness/sim_hal.hpp"

namespace harness {
namespace {

bool in_any_window(const Scenario& sc, std::uint32_t t, std::uint32_t pad_after) {
  for (const auto& f : sc.faults) {
    if (t >= f.start_ms && t < f.end_ms + pad_after) return true;
  }
  return false;
}

std::string ms(std::int64_t v) { return std::to_string(v) + " ms"; }

}  // namespace

bool ScenarioResult::passed() const {
  return std::all_of(checks.begin(), checks.end(), [](const Check& c) { return c.passed; });
}

ScenarioResult run_scenario(const Scenario& sc, const fw::Config& cfg, std::uint32_t seed,
                            const Limits& lim) {
  SimHal hal(sc, seed);
  fw::SensorController ctl(hal, cfg);
  for (std::uint32_t t = 0; t < sc.duration_ms; ++t) {
    hal.set_time(t);
    ctl.tick();
  }

  const DecodeResult dec = decode_stream(hal.wire());
  const auto& pk = dec.packets;

  ScenarioResult r;
  r.name = sc.name;
  r.packets_sent = static_cast<std::uint32_t>(hal.sent().size());
  r.packets_accepted = static_cast<std::uint32_t>(pk.size());
  r.packets_corrupted =
      static_cast<std::uint32_t>(std::count(hal.corrupted().begin(), hal.corrupted().end(), true));
  r.frames_rejected = dec.rejected_frames;

  // 1. Calibration finishes and the firmware reaches RUN in time.
  {
    auto it = std::find_if(pk.begin(), pk.end(),
                           [](const fw::Telemetry& p) { return p.state == fw::State::Run; });
    const bool ok = it != pk.end() && it->uptime_ms <= lim.boot_deadline_ms;
    r.checks.push_back({"boots_to_run", ok,
                        it == pk.end() ? "never reached RUN" : "RUN at " + ms(it->uptime_ms)});
  }

  // 2. Each injected fault is reported with the right code, fast enough,
  //    and the firmware recovers once the fault clears.
  for (std::size_t k = 0; k < sc.faults.size(); ++k) {
    const FaultWindow& f = sc.faults[k];
    const std::string tag = std::to_string(k + 1);
    auto det = std::find_if(pk.begin(), pk.end(), [&](const fw::Telemetry& p) {
      return p.uptime_ms >= f.start_ms && p.state == fw::State::Fault;
    });
    if (det == pk.end()) {
      r.checks.push_back({"fault_detected_" + tag, false,
                          std::string(to_string(f.type)) + " never detected"});
      r.checks.push_back({"recovered_" + tag, false, "no fault was reported"});
      continue;
    }
    const std::int32_t latency =
        static_cast<std::int32_t>(det->uptime_ms) - static_cast<std::int32_t>(f.start_ms);
    r.max_detection_latency_ms = std::max(r.max_detection_latency_ms, latency);
    const bool right_code = det->fault == expected_fault(f.type);
    const bool fast = latency <= static_cast<std::int32_t>(lim.max_detection_ms);
    r.checks.push_back({"fault_detected_" + tag, right_code && fast,
                        std::string(to_string(f.type)) + " -> " + fw::to_string(det->fault) +
                            " after " + ms(latency) + " (limit " + ms(lim.max_detection_ms) + ")"});

    auto rec = std::find_if(det, pk.end(), [&](const fw::Telemetry& p) {
      return p.uptime_ms >= f.end_ms && p.state == fw::State::Run;
    });
    if (rec == pk.end()) {
      r.checks.push_back({"recovered_" + tag, false, "never returned to RUN"});
    } else {
      const std::int32_t took =
          static_cast<std::int32_t>(rec->uptime_ms) - static_cast<std::int32_t>(f.end_ms);
      r.max_recovery_ms = std::max(r.max_recovery_ms, took);
      r.checks.push_back({"recovered_" + tag, took <= static_cast<std::int32_t>(lim.max_recovery_ms),
                          "RUN again " + ms(took) + " after the fault cleared"});
    }
  }

  // 3. No faults outside the injected windows.
  {
    std::uint32_t episodes = 0;
    bool prev_fault = false;
    for (const auto& p : pk) {
      const bool is_fault = p.state == fw::State::Fault;
      if (is_fault && !prev_fault && !in_any_window(sc, p.uptime_ms, lim.max_recovery_ms)) ++episodes;
      prev_fault = is_fault;
    }
    r.false_alarms = episodes;
    r.checks.push_back({"no_false_alarms", episodes == 0,
                        std::to_string(episodes) + " unexpected fault episodes"});
  }

  // 4. The CRC rejects every corrupted packet and every clean packet gets through unchanged.
  {
    std::vector<fw::PacketBytes> clean;
    for (std::size_t i = 0; i < hal.sent().size(); ++i) {
      if (!hal.corrupted()[i]) clean.push_back(hal.sent()[i]);
    }
    const bool ok = clean == dec.accepted_bytes;
    r.checks.push_back(
        {"telemetry_integrity", ok,
         ok ? std::to_string(r.packets_accepted) + " clean packets accepted, all " +
                  std::to_string(r.packets_corrupted) + " corrupted packets rejected"
            : "accepted packets do not match what the firmware sent"});
  }

  // 5. Telemetry keeps its 100 Hz schedule.
  {
    const double expected = static_cast<double>(sc.duration_ms) / cfg.telemetry_period_ms;
    const bool ok = r.packets_accepted >= lim.min_packet_ratio * expected;
    r.checks.push_back({"telemetry_rate", ok,
                        std::to_string(r.packets_accepted) + " packets vs " +
                            std::to_string(static_cast<int>(expected)) + " scheduled"});
  }

  // 6. The filtered output tracks the true signal outside fault windows.
  {
    std::int32_t worst = 0;
    for (const auto& p : pk) {
      if (p.state != fw::State::Run || p.uptime_ms < 200) continue;
      if (in_any_window(sc, p.uptime_ms, lim.max_recovery_ms + 20)) continue;
      const std::int32_t err = std::abs(static_cast<std::int32_t>(p.filtered) - true_signal(p.uptime_ms));
      worst = std::max(worst, err);
    }
    r.max_tracking_error = worst;
    r.checks.push_back({"tracking_error", worst <= lim.max_tracking_error,
                        "max error " + std::to_string(worst) + " (limit " +
                            std::to_string(lim.max_tracking_error) + ")"});
  }

  return r;
}

std::vector<ScenarioResult> run_all(const fw::Config& cfg, std::uint32_t seed, const std::string& only) {
  std::vector<ScenarioResult> out;
  for (const auto& sc : builtin_scenarios()) {
    if (only.empty() || sc.name == only) out.push_back(run_scenario(sc, cfg, seed));
  }
  return out;
}

}  // namespace harness
