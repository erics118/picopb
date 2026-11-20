#pragma once

#include <cstddef>
#include <cstdint>
#include <picopb/primitives.hpp>
#include <picopb/wire_format.hpp>
#include <span>

namespace ppb {

// writes wire format bytes into a caller's buffer
// a byte that doesn't fit is dropped and sets overflowed(), and the output is then incomplete
class Writer {
   private:
    constexpr void write_little_endian(std::uint64_t value, std::size_t bytes) {
        for (std::size_t i = 0; i < bytes; ++i) {
            put(static_cast<std::byte>(value & 0xff));
            value >>= 8;
        }
    }

    constexpr void write_varint(std::uint64_t value) {
        // the rest doesn't fit in 7 bits, so set the continuation bit
        while (value >= 0x80) {
            put(static_cast<std::byte>((value & 0x7f) | 0x80));
            value >>= 7;
        }

        put(static_cast<std::byte>(value));
    }

    constexpr void write_tag(std::uint32_t field_number, WireType wire) {
        write_varint((static_cast<std::uint64_t>(field_number) << 3) |
                     static_cast<std::uint8_t>(wire));
    }

    constexpr void put(std::byte b) {
        if (size_ == out_.size()) {
            overflowed_ = true;

            return;
        }

        out_[size_++] = b;
    }

    std::span<std::byte> out_;
    std::size_t size_ = 0;
    bool overflowed_ = false;

   public:
    constexpr explicit Writer(std::span<std::byte> out) : out_(out) {}

    // the number of bytes written so far
    [[nodiscard]] constexpr std::size_t size() const { return size_; }

    // whether a byte didn't fit and was dropped; filling the buffer exactly doesn't count
    [[nodiscard]] constexpr bool overflowed() const { return overflowed_; }

    // writes a field and value
    // if the field has implicit presence, a 0 is skipped
    template <Primitive S>
    constexpr void write(std::uint32_t field_number, cpp_type_t<S> value) {
        std::uint64_t bits = S::to_bits(value);

        if (bits == 0) {
            return;
        }

        write_tag(field_number, S::wire);

        if constexpr (S::wire == WireType::Varint) {
            write_varint(bits);
        } else {
            write_little_endian(bits, fixed_size(S::wire));
        }
    }
};

}  // namespace ppb
