#pragma once

#include <cstddef>
#include <cstdint>

namespace fw {

// CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF). Bitwise, no lookup table,
// so it fits on small parts with little flash.
std::uint16_t crc16_ccitt(const std::uint8_t* data, std::size_t len);

}  // namespace fw
