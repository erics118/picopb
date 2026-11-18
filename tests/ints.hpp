#pragma once

#include <cstdint>
#include <picopb/picopb.hpp>
#include <string_view>

// stands in for what the generator will emit from tests/proto/ints.proto
namespace picopb::test_messages::ints {

enum class Color : std::int32_t {
    COLOR_UNSPECIFIED = 0,
    RED = 1,
    NEGATIVE = -1,
};

struct Ints {
    static constexpr std::string_view full_name = "picopb.test_messages.ints.Ints";

    std::int32_t i32 = 0;
    std::int64_t i64 = 0;
    std::uint32_t u32 = 0;
    std::uint64_t u64 = 0;
    std::int32_t s32 = 0;
    std::int64_t s64 = 0;
    std::uint32_t fx32 = 0;
    std::uint64_t fx64 = 0;
    std::int32_t sfx32 = 0;
    std::int64_t sfx64 = 0;
    bool b = false;
    Color color = Color::COLOR_UNSPECIFIED;

    // writes every field, for ppb::encode
    constexpr void write(ppb::Writer& w) const {
        w.write_int32(1, i32);
        w.write_int64(2, i64);
        w.write_uint32(3, u32);
        w.write_uint64(4, u64);
        w.write_sint32(5, s32);
        w.write_sint64(6, s64);
        w.write_fixed32(7, fx32);
        w.write_fixed64(8, fx64);
        w.write_sfixed32(9, sfx32);
        w.write_sfixed64(10, sfx64);
        w.write_bool(11, b);
        w.write_enum(12, color);
    }
};

}  // namespace picopb::test_messages::ints
