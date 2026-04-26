#include "harness/decoder.hpp"

#include <algorithm>

namespace harness {

DecodeResult decode_stream(const std::vector<std::uint8_t>& wire) {
  DecodeResult out;
  std::size_t i = 0;
  while (i < wire.size()) {
    if (wire[i] != fw::kSync || i + fw::kPacketSize > wire.size()) {
      ++out.skipped_bytes;
      ++i;
      continue;
    }
    fw::Telemetry t;
    if (fw::decode(&wire[i], fw::kPacketSize, t)) {
      out.packets.push_back(t);
      fw::PacketBytes b{};
      std::copy_n(wire.begin() + static_cast<std::ptrdiff_t>(i), fw::kPacketSize, b.begin());
      out.accepted_bytes.push_back(b);
      i += fw::kPacketSize;
    } else {
      ++out.rejected_frames;
      ++i;  // resync: try the next byte
    }
  }
  return out;
}

}  // namespace harness
