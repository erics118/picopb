#pragma once

#include <bit>
#include <concepts>
#include <cstdint>
#include <expected>
#include <limits>
#include <picopb/error.hpp>
#include <picopb/types/field_type.hpp>
#include <picopb/wire/reader.hpp>
#include <picopb/wire/wire_type.hpp>
#include <picopb/wire/writer.hpp>
#include <type_traits>
#include <utility>

namespace ppb {

namespace detail {

// a number type: one C++ value written as one varint, i32 or i64 wire value
template <class S>
concept NumberType = requires(S::cpp_type value, std::uint64_t bits) {
    requires S::wire == WireType::Varint || fixed_size(S::wire) != 0;
    { S::to_bits(value) } -> std::same_as<std::uint64_t>;
    { S::from_bits(bits) } -> std::same_as<typename S::cpp_type>;
};

}  // namespace detail

struct Int32 {
    using cpp_type = std::int32_t;
    static constexpr detail::WireType wire = detail::WireType::Varint;

    // negative value is widened to 64 bits in two's complement
    static constexpr std::uint64_t to_bits(cpp_type value) {
        return static_cast<std::uint64_t>(value);
    }

    // an int32 is sign-extended to 64 bits when written, so its value is the low 32 bits
    static constexpr cpp_type from_bits(std::uint64_t bits) { return static_cast<cpp_type>(bits); }
};

struct Int64 {
    using cpp_type = std::int64_t;
    static constexpr detail::WireType wire = detail::WireType::Varint;

    static constexpr std::uint64_t to_bits(cpp_type value) {
        return static_cast<std::uint64_t>(value);
    }

    static constexpr cpp_type from_bits(std::uint64_t bits) { return static_cast<cpp_type>(bits); }
};

struct Uint32 {
    using cpp_type = std::uint32_t;
    static constexpr detail::WireType wire = detail::WireType::Varint;

    static constexpr std::uint64_t to_bits(cpp_type value) { return value; }

    static constexpr cpp_type from_bits(std::uint64_t bits) { return static_cast<cpp_type>(bits); }
};

struct Uint64 {
    using cpp_type = std::uint64_t;
    static constexpr detail::WireType wire = detail::WireType::Varint;

    static constexpr std::uint64_t to_bits(cpp_type value) { return value; }

    static constexpr cpp_type from_bits(std::uint64_t bits) { return bits; }
};

// zigzag encoding for sint32 and sint64
// n >= 0 gets turned into 2n; n < 0 becomes -2n-1
struct Sint32 {
    using cpp_type = std::int32_t;
    static constexpr detail::WireType wire = detail::WireType::Varint;

    static constexpr std::uint64_t to_bits(cpp_type value) {
        return (static_cast<std::uint32_t>(value) << 1) ^ static_cast<std::uint32_t>(value >> 31);
    }

    static constexpr cpp_type from_bits(std::uint64_t bits) {
        const auto n = static_cast<std::uint32_t>(bits);

        return static_cast<cpp_type>((n >> 1) ^ (0U - (n & 1U)));
    }
};

struct Sint64 {
    using cpp_type = std::int64_t;
    static constexpr detail::WireType wire = detail::WireType::Varint;

    static constexpr std::uint64_t to_bits(cpp_type value) {
        return (static_cast<std::uint64_t>(value) << 1) ^ static_cast<std::uint64_t>(value >> 63);
    }

    static constexpr cpp_type from_bits(std::uint64_t bits) {
        return static_cast<cpp_type>((bits >> 1) ^ (0U - (bits & 1U)));
    }
};

struct Fixed32 {
    using cpp_type = std::uint32_t;
    static constexpr detail::WireType wire = detail::WireType::I32;

    static constexpr std::uint64_t to_bits(cpp_type value) { return value; }

    static constexpr cpp_type from_bits(std::uint64_t bits) { return static_cast<cpp_type>(bits); }
};

struct Fixed64 {
    using cpp_type = std::uint64_t;
    static constexpr detail::WireType wire = detail::WireType::I64;

    static constexpr std::uint64_t to_bits(cpp_type value) { return value; }

    static constexpr cpp_type from_bits(std::uint64_t bits) { return bits; }
};

// sfixed32 and sfixed64 write the value's bits unchanged, with no zigzag or widening
struct Sfixed32 {
    using cpp_type = std::int32_t;
    static constexpr detail::WireType wire = detail::WireType::I32;

    static constexpr std::uint64_t to_bits(cpp_type value) {
        return static_cast<std::uint32_t>(value);
    }

    static constexpr cpp_type from_bits(std::uint64_t bits) { return static_cast<cpp_type>(bits); }
};

struct Sfixed64 {
    using cpp_type = std::int64_t;
    static constexpr detail::WireType wire = detail::WireType::I64;

    static constexpr std::uint64_t to_bits(cpp_type value) {
        return static_cast<std::uint64_t>(value);
    }

    static constexpr cpp_type from_bits(std::uint64_t bits) { return static_cast<cpp_type>(bits); }
};

struct Bool {
    using cpp_type = bool;
    static constexpr detail::WireType wire = detail::WireType::Varint;

    static constexpr std::uint64_t to_bits(cpp_type value) { return value ? 1U : 0U; }

    // any value other than 0 is true
    static constexpr cpp_type from_bits(std::uint64_t bits) { return bits != 0; }
};

// protobuf's double and float are ieee 754 binary64 and binary32, written as their bits
static_assert(std::numeric_limits<double>::is_iec559 && sizeof(double) == 8);
static_assert(std::numeric_limits<float>::is_iec559 && sizeof(float) == 4);

struct Double {
    using cpp_type = double;
    static constexpr detail::WireType wire = detail::WireType::I64;

    static constexpr std::uint64_t to_bits(cpp_type value) {
        return std::bit_cast<std::uint64_t>(value);
    }

    static constexpr cpp_type from_bits(std::uint64_t bits) {
        return std::bit_cast<cpp_type>(bits);
    }
};

struct Float {
    using cpp_type = float;
    static constexpr detail::WireType wire = detail::WireType::I32;

    static constexpr std::uint64_t to_bits(cpp_type value) {
        return std::bit_cast<std::uint32_t>(value);
    }

    static constexpr cpp_type from_bits(std::uint64_t bits) {
        return std::bit_cast<cpp_type>(static_cast<std::uint32_t>(bits));
    }
};

namespace detail {

// an enum is written as if it were an int32, and keeps values it has no name for
template <class E>
struct EnumNumber {
    using cpp_type = E;
    static constexpr WireType wire = WireType::Varint;

    static constexpr std::uint64_t to_bits(cpp_type value) {
        return Int32::to_bits(std::to_underlying(value));
    }

    static constexpr cpp_type from_bits(std::uint64_t bits) {
        return static_cast<cpp_type>(Int32::from_bits(bits));
    }
};

template <class S>
    requires NumberType<S>
struct FieldTypeTraits<S> {
    template <class V>
    static constexpr bool stores = std::same_as<V, typename S::cpp_type>;

    // a 0 isn't written, since the field has implicit presence
    static constexpr void write(Writer& w, std::uint32_t number, const S::cpp_type& value) {
        const std::uint64_t bits = S::to_bits(value);

        if (bits == 0) {
            return;
        }

        w.write_tag(number, S::wire);

        if constexpr (S::wire == WireType::Varint) {
            w.write_varint(bits);
        } else if constexpr (S::wire == WireType::I32) {
            w.write_i32(static_cast<std::uint32_t>(bits));
        } else {
            w.write_i64(bits);
        }
    }

    [[nodiscard]] static constexpr std::expected<bool, Error> read(Reader& r, WireType wire,
                                                                   S::cpp_type& value) {
        if (wire != S::wire) {
            return false;
        }

        std::expected<std::uint64_t, Error> bits = 0;

        if constexpr (S::wire == WireType::Varint) {
            bits = r.read_varint();
        } else if constexpr (S::wire == WireType::I32) {
            const auto i32 = r.read_i32();

            if (!i32) {
                return std::unexpected(i32.error());
            }

            bits = *i32;
        } else {
            bits = r.read_i64();
        }

        if (!bits) {
            return std::unexpected(bits.error());
        }

        value = S::from_bits(*bits);

        return true;
    }
};

// an enum is its own field type, when its values fit an int32 as protobuf requires
template <class E>
    requires std::is_enum_v<E> && std::same_as<std::underlying_type_t<E>, std::int32_t>
struct FieldTypeTraits<E> : FieldTypeTraits<EnumNumber<E>> {};

}  // namespace detail

}  // namespace ppb
