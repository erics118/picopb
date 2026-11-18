#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <picopb/error.hpp>
#include <picopb/wire_format.hpp>
#include <span>
#include <type_traits>

namespace ppb {

// reads bytes
class Reader {
   private:
    // reads a varint, i32 or i64 value, enums included, and stores from_bits(bits) in value
    // false means the field has another wire type, so the caller skips it like an unknown field
    template <WireType expected_wire, class T, class FromBits>
        requires(expected_wire != WireType::Len)
    constexpr std::expected<bool, Error> read_field(WireType wire, T& value, FromBits from_bits)
        requires requires(std::uint64_t bits) {
            { from_bits(bits) } -> std::same_as<T>;
        }
    {
        if (wire != expected_wire) {
            return false;
        }

        std::expected<std::uint64_t, Error> bits = 0;

        switch (expected_wire) {
            case WireType::Varint:
                bits = read_varint();
                break;
            case WireType::I32:
                bits = read_little_endian(4);
                break;
            case WireType::I64:
                bits = read_little_endian(8);
                break;
        }

        if (!bits) {
            return std::unexpected(bits.error());
        }

        value = from_bits(*bits);

        return true;
    }

    constexpr std::expected<std::uint64_t, Error> read_little_endian(std::size_t bytes) {
        if (bytes > in_.size() - pos_) {
            return std::unexpected(Error::Truncated);
        }

        std::uint64_t value = 0;

        for (std::size_t i = 0; i < bytes; ++i) {
            value |= std::to_integer<std::uint64_t>(in_[pos_++]) << (8 * i);
        }

        return value;
    }

    constexpr std::expected<void, Error> advance(std::uint64_t bytes) {
        if (bytes > in_.size() - pos_) {
            return std::unexpected(Error::Truncated);
        }

        pos_ += static_cast<std::size_t>(bytes);

        return {};
    }

    std::span<const std::byte> in_;
    std::size_t pos_ = 0;

   public:
    constexpr explicit Reader(std::span<const std::byte> in) : in_(in) {}

    // whether every byte has been read
    [[nodiscard]] constexpr bool done() const { return pos_ == in_.size(); }

    constexpr std::expected<std::uint64_t, Error> read_varint() {
        std::uint64_t value = 0;

        // 10 bytes of 7 bits each cover all 64 bits
        for (int shift = 0; shift < 70; shift += 7) {
            if (pos_ == in_.size()) {
                return std::unexpected(Error::Truncated);
            }

            auto b = std::to_integer<std::uint64_t>(in_[pos_++]);

            value |= (b & 0x7f) << shift;

            // no continuation bit, so this byte is the last
            if ((b & 0x80) == 0) {
                return value;
            }
        }

        return std::unexpected(Error::InvalidVarint);
    }

    constexpr std::expected<Tag, Error> read_tag() {
        auto bits = read_varint();

        if (!bits) {
            return std::unexpected(bits.error());
        }

        auto wire = static_cast<WireType>(*bits & 7);
        std::uint64_t field_number = *bits >> 3;
        bool known_wire = wire == WireType::Varint || wire == WireType::I64 ||
                          wire == WireType::Len || wire == WireType::I32;

        if (*bits > std::numeric_limits<std::uint32_t>::max() || field_number == 0 || !known_wire) {
            return std::unexpected(Error::InvalidTag);
        }

        return Tag{
            .field_number = static_cast<std::uint32_t>(field_number),
            .wire = wire,
        };
    }

    // steps over a field's value, for a field the message doesn't know
    constexpr std::expected<void, Error> skip(WireType wire) {
        switch (wire) {
            case WireType::Varint: {
                auto value = read_varint();

                if (!value) {
                    return std::unexpected(value.error());
                }

                return {};
            }
            case WireType::I64:
                return advance(8);
            case WireType::Len: {
                auto length = read_varint();

                if (!length) {
                    return std::unexpected(length.error());
                }

                return advance(*length);
            }
            case WireType::I32:
                return advance(4);
        }

        return std::unexpected(Error::InvalidTag);
    }

    // an int32 is sign-extended to 64 bits when written, so its value is the low 32 bits
    constexpr std::expected<bool, Error> read_int32(WireType wire, std::int32_t& value) {
        return read_field<WireType::Varint>(
            wire, value, [](std::uint64_t bits) { return static_cast<std::int32_t>(bits); });
    }

    constexpr std::expected<bool, Error> read_int64(WireType wire, std::int64_t& value) {
        return read_field<WireType::Varint>(
            wire, value, [](std::uint64_t bits) { return static_cast<std::int64_t>(bits); });
    }

    constexpr std::expected<bool, Error> read_uint32(WireType wire, std::uint32_t& value) {
        return read_field<WireType::Varint>(
            wire, value, [](std::uint64_t bits) { return static_cast<std::uint32_t>(bits); });
    }

    constexpr std::expected<bool, Error> read_uint64(WireType wire, std::uint64_t& value) {
        return read_field<WireType::Varint>(wire, value, [](std::uint64_t bits) { return bits; });
    }

    // undo zigzag32
    constexpr std::expected<bool, Error> read_sint32(WireType wire, std::int32_t& value) {
        return read_field<WireType::Varint>(wire, value, [](std::uint64_t bits) {
            auto n = static_cast<std::uint32_t>(bits);

            return static_cast<std::int32_t>((n >> 1) ^ (0U - (n & 1U)));
        });
    }

    // undo zigzag64
    constexpr std::expected<bool, Error> read_sint64(WireType wire, std::int64_t& value) {
        return read_field<WireType::Varint>(wire, value, [](std::uint64_t bits) {
            return static_cast<std::int64_t>((bits >> 1) ^ (0U - (bits & 1U)));
        });
    }

    constexpr std::expected<bool, Error> read_fixed32(WireType wire, std::uint32_t& value) {
        return read_field<WireType::I32>(
            wire, value, [](std::uint64_t bits) { return static_cast<std::uint32_t>(bits); });
    }

    constexpr std::expected<bool, Error> read_fixed64(WireType wire, std::uint64_t& value) {
        return read_field<WireType::I64>(wire, value, [](std::uint64_t bits) { return bits; });
    }

    constexpr std::expected<bool, Error> read_sfixed32(WireType wire, std::int32_t& value) {
        return read_field<WireType::I32>(
            wire, value, [](std::uint64_t bits) { return static_cast<std::int32_t>(bits); });
    }

    constexpr std::expected<bool, Error> read_sfixed64(WireType wire, std::int64_t& value) {
        return read_field<WireType::I64>(
            wire, value, [](std::uint64_t bits) { return static_cast<std::int64_t>(bits); });
    }

    // any value other than 0 is true
    constexpr std::expected<bool, Error> read_bool(WireType wire, bool& value) {
        return read_field<WireType::Varint>(wire, value,
                                            [](std::uint64_t bits) { return bits != 0; });
    }

    // an enum is read like an int32, and keeps values it has no name for
    template <class E>
        requires std::is_enum_v<E> && std::is_same_v<std::underlying_type_t<E>, std::int32_t>
    constexpr std::expected<bool, Error> read_enum(WireType wire, E& value) {
        return read_field<WireType::Varint>(wire, value, [](std::uint64_t bits) {
            return static_cast<E>(static_cast<std::int32_t>(bits));
        });
    }
};

}  // namespace ppb
