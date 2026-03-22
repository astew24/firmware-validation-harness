#include "fw/packet.hpp"

#include "fw/crc16.hpp"

namespace fw {
namespace {

void put_u16(std::uint8_t* p, std::uint16_t v) {
  p[0] = static_cast<std::uint8_t>(v & 0xFF);
  p[1] = static_cast<std::uint8_t>(v >> 8);
}

void put_u32(std::uint8_t* p, std::uint32_t v) {
  for (int i = 0; i < 4; ++i) p[i] = static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF);
}

std::uint16_t get_u16(const std::uint8_t* p) {
  return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}

std::uint32_t get_u32(const std::uint8_t* p) {
  std::uint32_t v = 0;
  for (int i = 3; i >= 0; --i) v = (v << 8) | p[i];
  return v;
}

}  // namespace

PacketBytes encode(const Telemetry& t) {
  PacketBytes b{};
  b[0] = kSync;
  b[1] = t.seq;
  b[2] = static_cast<std::uint8_t>(t.state);
  b[3] = static_cast<std::uint8_t>(t.fault);
  put_u16(&b[4], static_cast<std::uint16_t>(t.filtered));
  put_u16(&b[6], static_cast<std::uint16_t>(t.raw));
  put_u32(&b[8], t.uptime_ms);
  put_u16(&b[12], crc16_ccitt(b.data(), 12));
  return b;
}

bool decode(const std::uint8_t* bytes, std::size_t len, Telemetry& out) {
  if (len < kPacketSize || bytes[0] != kSync) return false;
  if (get_u16(&bytes[12]) != crc16_ccitt(bytes, 12)) return false;
  if (bytes[2] > static_cast<std::uint8_t>(State::Fault)) return false;
  if (bytes[3] > static_cast<std::uint8_t>(FaultCode::ImplausibleJump)) return false;
  out.seq = bytes[1];
  out.state = static_cast<State>(bytes[2]);
  out.fault = static_cast<FaultCode>(bytes[3]);
  out.filtered = static_cast<std::int16_t>(get_u16(&bytes[4]));
  out.raw = static_cast<std::int16_t>(get_u16(&bytes[6]));
  out.uptime_ms = get_u32(&bytes[8]);
  return true;
}

const char* to_string(State s) {
  switch (s) {
    case State::Boot: return "BOOT";
    case State::Calibrate: return "CALIBRATE";
    case State::Run: return "RUN";
    case State::Fault: return "FAULT";
  }
  return "?";
}

const char* to_string(FaultCode f) {
  switch (f) {
    case FaultCode::None: return "NONE";
    case FaultCode::SensorTimeout: return "SENSOR_TIMEOUT";
    case FaultCode::OutOfRange: return "OUT_OF_RANGE";
    case FaultCode::ImplausibleJump: return "IMPLAUSIBLE_JUMP";
  }
  return "?";
}

}  // namespace fw
