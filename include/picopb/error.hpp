#pragma once

#include <cstdint>

namespace ppb {

// why an encode or decode failed
enum class Error : std::uint8_t {
    // the message's encoding needs more bytes than the buffer has
    BufferTooSmall,

    // input ends in the middle of a field
    Truncated,

    // varint runs past 10 bytes
    InvalidVarint,

    // tag doesn't fit in 32 bits, has field number 0, or has wire type other than the ones defined
    InvalidTag,
};

}  // namespace ppb
