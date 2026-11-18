#pragma once

#include <cstddef>
#include <cstdint>
#include <picopb/wire_format.hpp>
#include <span>
#include <type_traits>
#include <utility>

namespace ppb {

// writes wire format bytes into a caller's buffer
// a byte that doesn't fit is dropped and sets overflowed(), and the output is then incomplete
class Writer {
   private:
    // writes a field and value
    // if the field has implicit presence, a 0 is skipped
    template <WireType wire>
        requires(wire != WireType::Len)
    constexpr void write_field(std::uint32_t field_number, std::uint64_t bits) {
        if (bits == 0) {
            return;
        }

        write_tag(field_number, wire);

        switch (wire) {
            case WireType::Varint:
                write_varint(bits);
                break;
            case WireType::I32:
                write_i32(static_cast<std::uint32_t>(bits));
                break;
            case WireType::I64:
                write_i64(bits);
                break;
        }
    }

    constexpr void write_little_endian(std::uint64_t value, int bytes) {
        for (int i = 0; i < bytes; ++i) {
            put(static_cast<std::byte>(value & 0xff));
            value >>= 8;
        }
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

    constexpr void write_varint(std::uint64_t value) {
        // the rest doesn't fit in 7 bits, so set the continuation bit
        while (value >= 0x80) {
            put(static_cast<std::byte>((value & 0x7f) | 0x80));
            value >>= 7;
        }

        put(static_cast<std::byte>(value));
    }

    constexpr void write_i32(std::uint32_t value) { write_little_endian(value, 4); }

    constexpr void write_i64(std::uint64_t value) { write_little_endian(value, 8); }

    constexpr void write_tag(std::uint32_t field_number, WireType wire) {
        write_varint((static_cast<std::uint64_t>(field_number) << 3) |
                     static_cast<std::uint8_t>(wire));
    }

    // negative value is widened to 64 bits in two's complement
    constexpr void write_int32(std::uint32_t field_number, std::int32_t value) {
        write_field<WireType::Varint>(field_number, static_cast<std::uint64_t>(value));
    }

    constexpr void write_int64(std::uint32_t field_number, std::int64_t value) {
        write_field<WireType::Varint>(field_number, static_cast<std::uint64_t>(value));
    }

    constexpr void write_uint32(std::uint32_t field_number, std::uint32_t value) {
        write_field<WireType::Varint>(field_number, value);
    }

    constexpr void write_uint64(std::uint32_t field_number, std::uint64_t value) {
        write_field<WireType::Varint>(field_number, value);
    }

    constexpr void write_sint32(std::uint32_t field_number, std::int32_t value) {
        write_field<WireType::Varint>(field_number, zigzag32(value));
    }

    constexpr void write_sint64(std::uint32_t field_number, std::int64_t value) {
        write_field<WireType::Varint>(field_number, zigzag64(value));
    }

    constexpr void write_fixed32(std::uint32_t field_number, std::uint32_t value) {
        write_field<WireType::I32>(field_number, value);
    }

    constexpr void write_fixed64(std::uint32_t field_number, std::uint64_t value) {
        write_field<WireType::I64>(field_number, value);
    }

    // sfixed32 and sfixed64 write the value's bits unchanged, with no ZigZag or widening
    constexpr void write_sfixed32(std::uint32_t field_number, std::int32_t value) {
        write_field<WireType::I32>(field_number, static_cast<std::uint32_t>(value));
    }

    constexpr void write_sfixed64(std::uint32_t field_number, std::int64_t value) {
        write_field<WireType::I64>(field_number, static_cast<std::uint64_t>(value));
    }

    constexpr void write_bool(std::uint32_t field_number, bool value) {
        write_field<WireType::Varint>(field_number, value ? 1U : 0U);
    }

    // enums are encoded as if it were an int32 (ie as a varint)
    // values fit inside a std::int32_t, so they are generated as so
    // `requires` that E is an enum and matches the type of a std::uint32_t
    template <class E>
        requires std::is_enum_v<E> && std::is_same_v<std::underlying_type_t<E>, std::int32_t>
    constexpr void write_enum(std::uint32_t field_number, E value) {
        write_int32(field_number, std::to_underlying(value));
    }
};

}  // namespace ppb
