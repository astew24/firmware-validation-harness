#pragma once

#include <map>
#include <string>
#include <vector>

#include "harness/validator.hpp"

namespace harness {

// Flat "scenario/metric" -> value map, saved as CSV.
using MetricMap = std::map<std::string, double>;

MetricMap flatten(const std::vector<ScenarioResult>& results);
bool write_metrics(const std::string& path, const MetricMap& m);
bool read_metrics(const std::string& path, MetricMap& m);

struct Regression {
  std::string key;
  double baseline = 0;
  double current = 0;
  std::string reason;
};

// A regression is a check that passed in the baseline and fails now, or a
// metric that got meaningfully worse (slower detection or recovery, new false
// alarms, larger tracking error).
std::vector<Regression> compare(const MetricMap& baseline, const MetricMap& current);

}  // namespace harness
