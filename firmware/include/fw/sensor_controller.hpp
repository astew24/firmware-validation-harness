#pragma once

#include <cstddef>
#include <cstdint>

#include "fw/packet.hpp"
#include "fw/ring_buffer.hpp"

namespace fw {

// Hardware abstraction layer. On a microcontroller this wraps the ADC, a
// millisecond timer and a UART. The test harness plugs in a simulated one.
class Hal {
 public:
  virtual ~Hal() = default;
  // Returns false when no new sample is ready this tick.
  virtual bool read_sensor(std::int16_t& out) = 0;
  virtual std::uint32_t now_ms() const = 0;
  virtual void transmit(const std::uint8_t* data, std::size_t len) = 0;
};

struct Config {
  std::uint16_t calibration_samples = 64;  // samples averaged at rest to find the sensor bias
  std::uint16_t sensor_timeout_ms = 20;    // no sample for this long = sensor timeout fault
  std::int16_t min_valid = -2000;          // raw reading limits
  std::int16_t max_valid = 2000;
  std::int16_t max_jump = 400;             // largest believable change between two good samples
  std::uint16_t recovery_samples = 50;     // good samples in a row needed to leave FAULT
  std::uint16_t telemetry_period_ms = 10;  // 100 Hz telemetry
};

// Sensor controller firmware. Call tick() once per millisecond from the main
// loop or a timer interrupt. Fixed-size storage only: no heap, no exceptions.
//
//   BOOT -> CALIBRATE -> RUN <-> FAULT
//
// Faults: sensor timeout, out-of-range reading, implausible jump between samples.
// Leaving FAULT takes `recovery_samples` good readings in a row.
class SensorController {
 public:
  explicit SensorController(Hal& hal, const Config& cfg = Config{});

  void tick();

  State state() const { return state_; }
  FaultCode fault() const { return fault_; }
  std::int16_t offset() const { return offset_; }
  std::int16_t filtered() const { return filtered_; }

 private:
  void step(std::uint32_t now, bool got, std::int16_t sample);
  bool plausible(std::int16_t v, FaultCode& why) const;
  void enter(State s, FaultCode f);
  void send_telemetry(std::uint32_t now);

  Hal& hal_;
  Config cfg_;
  State state_ = State::Boot;
  FaultCode fault_ = FaultCode::None;
  RingBuffer<std::int16_t, 8> window_;  // moving-average filter
  std::int32_t calib_sum_ = 0;
  std::uint16_t calib_count_ = 0;
  bool calibrated_ = false;
  std::int16_t offset_ = 0;
  std::int16_t filtered_ = 0;
  std::int16_t last_sample_ = 0;  // latest raw reading, sent in telemetry
  std::int16_t ref_ = 0;          // last good reading, used by the jump check
  bool have_ref_ = false;
  std::uint32_t last_sample_ms_ = 0;
  std::uint32_t last_tx_ms_ = 0;
  std::uint16_t good_streak_ = 0;
  std::uint8_t seq_ = 0;
};

}  // namespace fw
