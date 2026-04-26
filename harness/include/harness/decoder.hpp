#pragma once

#include <cstdint>
#include <vector>

#include "fw/packet.hpp"

namespace harness {

struct DecodeResult {
  std::vector<fw::Telemetry> packets;
  std::vector<fw::PacketBytes> accepted_bytes;
  std::uint32_t rejected_frames = 0;  // sync byte found but CRC or fields were bad
  std::uint32_t skipped_bytes = 0;    // bytes skipped while hunting for a sync byte
};

// Parses a raw UART byte stream: finds sync bytes, checks the CRC, and
// resyncs byte by byte after a bad frame.
DecodeResult decode_stream(const std::vector<std::uint8_t>& wire);

}  // namespace harness
