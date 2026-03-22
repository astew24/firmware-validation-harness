#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace fw {

enum class State : std::uint8_t { Boot = 0, Calibrate = 1, Run = 2, Fault = 3 };
enum class FaultCode : std::uint8_t { None = 0, SensorTimeout = 1, OutOfRange = 2, ImplausibleJump = 3 };

// Telemetry frame on the wire (14 bytes, little-endian):
//   [0] sync 0xAA  [1] seq  [2] state  [3] fault
//   [4..5] filtered  [6..7] raw  [8..11] uptime_ms  [12..13] CRC-16 of bytes 0..11
constexpr std::uint8_t kSync = 0xAA;
constexpr std::size_t kPacketSize = 14;

struct Telemetry {
  std::uint8_t seq = 0;
  State state = State::Boot;
  FaultCode fault = FaultCode::None;
  std::int16_t filtered = 0;
  std::int16_t raw = 0;
  std::uint32_t uptime_ms = 0;
};

using PacketBytes = std::array<std::uint8_t, kPacketSize>;

PacketBytes encode(const Telemetry& t);

// Returns false if the sync byte, CRC or enum fields are wrong.
bool decode(const std::uint8_t* bytes, std::size_t len, Telemetry& out);

const char* to_string(State s);
const char* to_string(FaultCode f);

}  // namespace fw
