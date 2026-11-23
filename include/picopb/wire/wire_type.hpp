#pragma once

#include <cstddef>
#include <cstdint>

namespace ppb {

// wire type determines how a field's value is encoded
// stored in the lowest 3 bits of the tag
// 3 and 4 are deprecated
enum class WireType : std::uint8_t {
    // base 128 varint, 1-10 bytes
    // lowest 7 bits are data, MSB is a continuation bit
    // used for: int32, int64, uint32, uint64, sint32, sint64, bool, enum
    Varint = 0,

    // 8 bytes
    // used for: fixed64, sfixed64, double
    I64 = 1,

    // a length (encoded as a varint), then that many bytes of data
    // used for: string, bytes, embedded messages, packed repeated fields
    Len = 2,

    // 4 bytes
    // used for: fixed32, sfixed32, float
    I32 = 5,
};

// if the wire type is supported
constexpr bool is_supported(WireType wire) {
    switch (wire) {
        case WireType::Varint:
        case WireType::I64:
        case WireType::Len:
        case WireType::I32:
            return true;
    }

    return false;
}

// how many bytes a value of this wire type takes, or 0 if its size isn't fixed
constexpr std::size_t fixed_size(WireType wire) {
    switch (wire) {
        case WireType::I32:
            return 4;
        case WireType::I64:
            return 8;
        case WireType::Varint:
        case WireType::Len:
            return 0;
    }

    return 0;
}

// the field number and wire type at the start of every field
struct Tag {
    std::uint32_t field_number;
    WireType wire;
};

}  // namespace ppb
