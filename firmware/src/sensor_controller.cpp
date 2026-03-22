#include "fw/sensor_controller.hpp"

namespace fw {

SensorController::SensorController(Hal& hal, const Config& cfg) : hal_(hal), cfg_(cfg) {}

void SensorController::tick() {
  const std::uint32_t now = hal_.now_ms();
  std::int16_t sample = 0;
  const bool got = hal_.read_sensor(sample);
  if (got) last_sample_ = sample;

  switch (state_) {
    case State::Boot:
      last_sample_ms_ = now;
      enter(State::Calibrate, FaultCode::None);
      break;

    case State::Calibrate:
      if (got) {
        last_sample_ms_ = now;
        calib_sum_ += sample;
        ++calib_count_;
        if (calib_count_ >= cfg_.calibration_samples) {
          offset_ = static_cast<std::int16_t>(calib_sum_ / calib_count_);
          enter(State::Run, FaultCode::None);
        }
      } else if (now - last_sample_ms_ >= cfg_.sensor_timeout_ms) {
        enter(State::Fault, FaultCode::SensorTimeout);
      }
      break;

    case State::Run:
    case State::Fault:
      step(now, got, sample);
      break;
  }

  if (now - last_tx_ms_ >= cfg_.telemetry_period_ms) send_telemetry(now);
}

void SensorController::step(std::uint32_t now, bool got, std::int16_t sample) {
  FaultCode why = FaultCode::None;
  bool good = false;

  if (got) {
    last_sample_ms_ = now;
    good = plausible(sample, why);
    // Only good readings become the reference for the jump check.
    if (good) {
      ref_ = sample;
      have_ref_ = true;
    } else {
      have_ref_ = false;
    }
  } else if (now - last_sample_ms_ >= cfg_.sensor_timeout_ms) {
    why = FaultCode::SensorTimeout;
    // After a gap the signal has moved on, so the next reading is not
    // compared with stale data (that would look like a spike).
    have_ref_ = false;
  }

  if (state_ == State::Run) {
    if (why != FaultCode::None) {
      enter(State::Fault, why);
    } else if (good) {
      window_.push(static_cast<std::int16_t>(sample - offset_));
      filtered_ = static_cast<std::int16_t>(window_.mean());
    }
    return;
  }

  // FAULT: wait for a streak of good readings.
  if (why != FaultCode::None) {
    good_streak_ = 0;
    return;
  }
  if (good && ++good_streak_ >= cfg_.recovery_samples) {
    if (!calibrated_) {
      enter(State::Calibrate, FaultCode::None);
      return;
    }
    window_.clear();
    window_.push(static_cast<std::int16_t>(sample - offset_));
    filtered_ = static_cast<std::int16_t>(window_.mean());
    enter(State::Run, FaultCode::None);
  }
}

bool SensorController::plausible(std::int16_t v, FaultCode& why) const {
  if (v < cfg_.min_valid || v > cfg_.max_valid) {
    why = FaultCode::OutOfRange;
    return false;
  }
  if (have_ref_) {
    const int delta = static_cast<int>(v) - static_cast<int>(ref_);
    if (delta > cfg_.max_jump || delta < -cfg_.max_jump) {
      why = FaultCode::ImplausibleJump;
      return false;
    }
  }
  return true;
}

void SensorController::enter(State s, FaultCode f) {
  state_ = s;
  fault_ = f;
  good_streak_ = 0;
  if (s == State::Calibrate) {
    calib_sum_ = 0;
    calib_count_ = 0;
  }
  if (s == State::Run) calibrated_ = true;
  // State changes go out right away instead of waiting for the next 10 ms slot.
  send_telemetry(hal_.now_ms());
}

void SensorController::send_telemetry(std::uint32_t now) {
  Telemetry t;
  t.seq = seq_++;
  t.state = state_;
  t.fault = fault_;
  t.filtered = filtered_;
  t.raw = last_sample_;
  t.uptime_ms = now;
  const PacketBytes bytes = encode(t);
  hal_.transmit(bytes.data(), bytes.size());
  last_tx_ms_ = now;
}

}  // namespace fw
