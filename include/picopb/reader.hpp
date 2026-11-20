#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <picopb/error.hpp>
#include <picopb/primitives.hpp>
#include <picopb/wire_format.hpp>
#include <span>

namespace ppb {

// reads bytes
class Reader {
   private:
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

    std::span<const std::byte> in_;
    std::size_t pos_ = 0;

   public:
    constexpr explicit Reader(std::span<const std::byte> in) : in_(in) {}

    // whether every byte has been read
    [[nodiscard]] constexpr bool done() const { return pos_ == in_.size(); }

    constexpr std::expected<Tag, Error> read_tag() {
        auto bits = read_varint();

        if (!bits) {
            return std::unexpected(bits.error());
        }

        auto wire = static_cast<WireType>(*bits & 7);
        std::uint64_t field_number = *bits >> 3;

        if (*bits > std::numeric_limits<std::uint32_t>::max() || field_number == 0 ||
            !is_supported(wire)) {
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
            case WireType::Len: {
                auto length = read_varint();

                if (!length) {
                    return std::unexpected(length.error());
                }

                return advance(*length);
            }
            case WireType::I32:
            case WireType::I64:
                return advance(fixed_size(wire));
        }

        return std::unexpected(Error::InvalidTag);
    }

    // reads a varint, i32 or i64 value, enums included, and stores S::from_bits(bits) in value
    // false means the field has another wire type, so the caller skips it like an unknown field
    template <Primitive S>
    constexpr std::expected<bool, Error> read(WireType wire, cpp_type_t<S>& value) {
        if (wire != S::wire) {
            return false;
        }

        std::expected<std::uint64_t, Error> bits = 0;

        if constexpr (S::wire == WireType::Varint) {
            bits = read_varint();
        } else {
            bits = read_little_endian(fixed_size(S::wire));
        }

        if (!bits) {
            return std::unexpected(bits.error());
        }

        value = S::from_bits(*bits);

        return true;
    }
};

}  // namespace ppb
