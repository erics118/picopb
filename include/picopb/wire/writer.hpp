#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <picopb/types/numbers.hpp>
#include <picopb/wire/wire_type.hpp>
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

    // copies bytes in bulk; a part that doesn't fit is dropped and sets overflowed()
    template <class Byte>
    constexpr void put_bytes(std::span<const Byte> bytes) {
        const std::size_t fits = std::min(bytes.size(), out_.size() - size_);

        // memcpy can't run at compile time, and the optimizer doesn't always turn the loop into it
        // fits > 0 keeps a null data() from an empty span out of memcpy
        if consteval {
            std::ranges::transform(bytes.first(fits), out_.subspan(size_).begin(), [](Byte b) {
                return static_cast<std::byte>(b);
            });
        } else {
            if (fits > 0) {
                std::memcpy(out_.subspan(size_).data(), bytes.data(), fits);
            }
        }

        size_ += fits;

        if (fits < bytes.size()) {
            overflowed_ = true;
        }
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

    // writes a field and value, even a 0; skipping a 0 is the field's job
    template <Primitive S>
    constexpr void write(std::uint32_t field_number, cpp_type_t<S> value) {
        const std::uint64_t bits = S::to_bits(value);

        write_tag(field_number, S::wire);

        if constexpr (S::wire == WireType::Varint) {
            write_varint(bits);
        } else {
            write_little_endian(bits, fixed_size(S::wire));
        }
    }

    // writes a length-delimited field: its tag, the length, then the bytes
    template <class Byte>
        requires(sizeof(Byte) == 1)
    constexpr void write_bytes(std::uint32_t field_number, std::span<const Byte> bytes) {
        write_tag(field_number, WireType::Len);
        write_varint(bytes.size());
        put_bytes(bytes);
    }
};

}  // namespace ppb
