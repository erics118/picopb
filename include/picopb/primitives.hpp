#pragma once

#include <concepts>
#include <cstdint>
#include <picopb/wire_format.hpp>
#include <type_traits>
#include <utility>

namespace ppb {

// the C++ type that holds a value of primitive type S, such as std::int32_t for ppb::Sint32
template <class S>
using cpp_type_t = S::cpp_type;

// a struct that represents a protobuf primitive type
// has a function defining how to convert to and from its wire format
template <class S>
concept Primitive = requires(cpp_type_t<S> value, std::uint64_t bits) {
    requires S::wire == WireType::Varint || fixed_size(S::wire) != 0;
    { S::to_bits(value) } -> std::same_as<std::uint64_t>;
    { S::from_bits(bits) } -> std::same_as<cpp_type_t<S>>;
};

struct Int32 {
    using cpp_type = std::int32_t;
    static constexpr WireType wire = WireType::Varint;

    // negative value is widened to 64 bits in two's complement
    static constexpr std::uint64_t to_bits(cpp_type value) {
        return static_cast<std::uint64_t>(value);
    }

    // an int32 is sign-extended to 64 bits when written, so its value is the low 32 bits
    static constexpr cpp_type from_bits(std::uint64_t bits) { return static_cast<cpp_type>(bits); }
};

struct Int64 {
    using cpp_type = std::int64_t;
    static constexpr WireType wire = WireType::Varint;

    static constexpr std::uint64_t to_bits(cpp_type value) {
        return static_cast<std::uint64_t>(value);
    }

    static constexpr cpp_type from_bits(std::uint64_t bits) { return static_cast<cpp_type>(bits); }
};

struct Uint32 {
    using cpp_type = std::uint32_t;
    static constexpr WireType wire = WireType::Varint;

    static constexpr std::uint64_t to_bits(cpp_type value) { return value; }

    static constexpr cpp_type from_bits(std::uint64_t bits) { return static_cast<cpp_type>(bits); }
};

struct Uint64 {
    using cpp_type = std::uint64_t;
    static constexpr WireType wire = WireType::Varint;

    static constexpr std::uint64_t to_bits(cpp_type value) { return value; }

    static constexpr cpp_type from_bits(std::uint64_t bits) { return bits; }
};

// zigzag encoding for sint32 and sint64
// n >= 0 gets turned into 2n; n < 0 becomes -2n-1
struct Sint32 {
    using cpp_type = std::int32_t;
    static constexpr WireType wire = WireType::Varint;

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
    static constexpr WireType wire = WireType::Varint;

    static constexpr std::uint64_t to_bits(cpp_type value) {
        return (static_cast<std::uint64_t>(value) << 1) ^ static_cast<std::uint64_t>(value >> 63);
    }

    static constexpr cpp_type from_bits(std::uint64_t bits) {
        return static_cast<cpp_type>((bits >> 1) ^ (0U - (bits & 1U)));
    }
};

struct Fixed32 {
    using cpp_type = std::uint32_t;
    static constexpr WireType wire = WireType::I32;

    static constexpr std::uint64_t to_bits(cpp_type value) { return value; }

    static constexpr cpp_type from_bits(std::uint64_t bits) { return static_cast<cpp_type>(bits); }
};

struct Fixed64 {
    using cpp_type = std::uint64_t;
    static constexpr WireType wire = WireType::I64;

    static constexpr std::uint64_t to_bits(cpp_type value) { return value; }

    static constexpr cpp_type from_bits(std::uint64_t bits) { return bits; }
};

// sfixed32 and sfixed64 write the value's bits unchanged, with no zigzag or widening
struct Sfixed32 {
    using cpp_type = std::int32_t;
    static constexpr WireType wire = WireType::I32;

    static constexpr std::uint64_t to_bits(cpp_type value) {
        return static_cast<std::uint32_t>(value);
    }

    static constexpr cpp_type from_bits(std::uint64_t bits) { return static_cast<cpp_type>(bits); }
};

struct Sfixed64 {
    using cpp_type = std::int64_t;
    static constexpr WireType wire = WireType::I64;

    static constexpr std::uint64_t to_bits(cpp_type value) {
        return static_cast<std::uint64_t>(value);
    }

    static constexpr cpp_type from_bits(std::uint64_t bits) { return static_cast<cpp_type>(bits); }
};

struct Bool {
    using cpp_type = bool;
    static constexpr WireType wire = WireType::Varint;

    static constexpr std::uint64_t to_bits(cpp_type value) { return value ? 1U : 0U; }

    // any value other than 0 is true
    static constexpr cpp_type from_bits(std::uint64_t bits) { return bits != 0; }
};

// enums are encoded as if it were an int32 (ie as a varint)
// values fit inside a std::int32_t, so they are generated as so
// `requires` that E is an enum and matches the type of a std::int32_t
template <class E>
    requires std::is_enum_v<E> && std::is_same_v<std::underlying_type_t<E>, std::int32_t>
struct Enum {
    using cpp_type = E;
    static constexpr WireType wire = WireType::Varint;

    static constexpr std::uint64_t to_bits(cpp_type value) {
        return Int32::to_bits(std::to_underlying(value));
    }

    // an enum is read like an int32, and keeps values it has no name for
    static constexpr cpp_type from_bits(std::uint64_t bits) {
        return static_cast<cpp_type>(Int32::from_bits(bits));
    }
};

}  // namespace ppb
