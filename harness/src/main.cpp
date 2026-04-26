// fvh: runs the firmware validation scenarios, prints a report, compares the
// results with a saved baseline, and can serve live metrics for Prometheus.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <string>
#include <thread>

#include "harness/baseline.hpp"
#include "harness/metrics.hpp"
#include "harness/validator.hpp"
#include "metrics_server.hpp"

namespace {

struct Options {
  std::string scenario;
  std::uint32_t seed = 1;
  int fw_timeout_ms = 0;
  std::string baseline = "baseline/baseline.csv";
  std::string results = "latest_results.csv";
  bool update_baseline = false;
  bool verbose = false;
  int serve_port = 0;
  int interval_sec = 2;
  int bad_build_every = 0;
};

void usage() {
  std::puts(
      "usage: fvh [options]\n"
      "  --scenario NAME        run one scenario (default: all)\n"
      "  --seed N               noise and corruption seed (default 1)\n"
      "  --fw-timeout-ms N      override the firmware sensor timeout (simulates a firmware change)\n"
      "  --baseline PATH        baseline CSV to compare against (default baseline/baseline.csv)\n"
      "  --results PATH         where to write this run's metrics (default latest_results.csv)\n"
      "  --update-baseline      save this run as the new baseline (only if everything passes)\n"
      "  --verbose              print every check, not just failures\n"
      "  --serve PORT           keep running scenarios and serve Prometheus metrics on PORT\n"
      "  --interval-sec N       seconds between cycles in serve mode (default 2)\n"
      "  --bad-build-every N    in serve mode, run a slowed firmware build for 4 of every N cycles");
}

bool parse(int argc, char** argv, Options& o) {
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto value = [&]() -> std::string { return i + 1 < argc ? argv[++i] : ""; };
    if (a == "--scenario") o.scenario = value();
    else if (a == "--seed") o.seed = static_cast<std::uint32_t>(std::stoul(value()));
    else if (a == "--fw-timeout-ms") o.fw_timeout_ms = std::stoi(value());
    else if (a == "--baseline") o.baseline = value();
    else if (a == "--results") o.results = value();
    else if (a == "--update-baseline") o.update_baseline = true;
    else if (a == "--verbose") o.verbose = true;
    else if (a == "--serve") o.serve_port = std::stoi(value());
    else if (a == "--interval-sec") o.interval_sec = std::stoi(value());
    else if (a == "--bad-build-every") o.bad_build_every = std::stoi(value());
    else if (a == "-h" || a == "--help") { usage(); std::exit(0); }
    else { std::cerr << "unknown option: " << a << "\n"; return false; }
  }
  return true;
}

void print_report(const std::vector<harness::ScenarioResult>& rs, bool verbose) {
  std::printf("%-16s %-6s %9s %10s %6s %6s %12s %8s\n", "SCENARIO", "RESULT", "DETECT_MS", "RECOVER_MS",
              "FALSE", "TRACK", "PKTS_OK/SENT", "CORRUPT");
  for (const auto& r : rs) {
    const std::string det = r.max_detection_latency_ms < 0 ? "-" : std::to_string(r.max_detection_latency_ms);
    const std::string rec = r.max_recovery_ms < 0 ? "-" : std::to_string(r.max_recovery_ms);
    const std::string pk = std::to_string(r.packets_accepted) + "/" + std::to_string(r.packets_sent);
    std::printf("%-16s %-6s %9s %10s %6u %6d %12s %8u\n", r.name.c_str(), r.passed() ? "PASS" : "FAIL",
                det.c_str(), rec.c_str(), static_cast<unsigned>(r.false_alarms),
                static_cast<int>(r.max_tracking_error), pk.c_str(), static_cast<unsigned>(r.packets_corrupted));
    for (const auto& c : r.checks) {
      if (!c.passed || verbose) {
        std::printf("    %s %-20s %s\n", c.passed ? "ok  " : "FAIL", c.name.c_str(), c.detail.c_str());
      }
    }
  }
}

int serve(const Options& o, const fw::Config& base_cfg) {
  harness::MetricsRegistry registry;
  std::atomic<bool> running{true};

  std::thread worker([&]() {
    std::uint32_t cycle = 0;
    while (running.load()) {
      // Every N cycles, run a slowed build for 4 cycles in a row: long enough
      // that a 5 s Prometheus scrape always sees it.
      const auto every = static_cast<std::uint32_t>(std::max(o.bad_build_every, 0));
      const bool bad = every > 0 && cycle % every >= every - std::min<std::uint32_t>(4, every);
      fw::Config cfg = base_cfg;
      if (bad) cfg.sensor_timeout_ms = 60;  // a firmware change that slows fault detection
      const auto results = harness::run_all(cfg, o.seed + cycle, o.scenario);
      const double now =
          std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
      registry.record_cycle(results, bad, now);
      ++cycle;
      for (int i = 0; i < o.interval_sec * 10 && running.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
    }
  });

  std::printf("serving metrics on http://127.0.0.1:%d/metrics\n", o.serve_port);
  std::fflush(stdout);
  const bool ok = serve_metrics(o.serve_port, [&]() { return registry.render(); }, running);
  running = false;
  worker.join();
  if (!ok) {
    std::fprintf(stderr, "could not open port %d\n", o.serve_port);
    return 1;
  }
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  Options o;
  try {
    if (!parse(argc, argv, o)) {
      usage();
      return 2;
    }
  } catch (const std::exception&) {
    usage();
    return 2;
  }

  fw::Config cfg;
  if (o.fw_timeout_ms > 0) cfg.sensor_timeout_ms = static_cast<std::uint16_t>(o.fw_timeout_ms);
  if (o.serve_port > 0) return serve(o, cfg);

  const auto results = harness::run_all(cfg, o.seed, o.scenario);
  if (results.empty()) {
    std::cerr << "no scenario named " << o.scenario << "\n";
    return 2;
  }
  print_report(results, o.verbose);

  const auto current = harness::flatten(results);
  if (!o.results.empty() && !harness::write_metrics(o.results, current)) {
    std::cerr << "could not write " << o.results << "\n";
  }
  const bool all_passed = std::all_of(results.begin(), results.end(),
                                      [](const harness::ScenarioResult& r) { return r.passed(); });

  if (o.update_baseline) {
    if (!all_passed) {
      std::cerr << "\nrefusing to save a baseline with failing checks\n";
      return 1;
    }
    const auto dir = std::filesystem::path(o.baseline).parent_path();
    if (!dir.empty()) std::filesystem::create_directories(dir);
    if (!harness::write_metrics(o.baseline, current)) {
      std::cerr << "could not write " << o.baseline << "\n";
      return 1;
    }
    std::printf("\nbaseline saved to %s\n", o.baseline.c_str());
    return 0;
  }

  int rc = all_passed ? 0 : 1;
  harness::MetricMap base;
  if (harness::read_metrics(o.baseline, base)) {
    if (!o.scenario.empty()) {
      const std::string prefix = o.scenario + "/";
      for (auto it = base.begin(); it != base.end();) {
        it = it->first.rfind(prefix, 0) == 0 ? std::next(it) : base.erase(it);
      }
    }
    const auto regressions = harness::compare(base, current);
    if (regressions.empty()) {
      std::printf("\nbaseline %s: no regressions\n", o.baseline.c_str());
    } else {
      std::printf("\nREGRESSIONS vs %s:\n", o.baseline.c_str());
      for (const auto& r : regressions) {
        std::printf("  %-46s %6g -> %-6g %s\n", r.key.c_str(), r.baseline, r.current, r.reason.c_str());
      }
      rc = 1;
    }
  } else {
    std::printf("\n(no baseline at %s; run with --update-baseline to create one)\n", o.baseline.c_str());
  }
  std::printf("RESULT: %s\n", rc == 0 ? "PASS" : "FAIL");
  return rc;
}
