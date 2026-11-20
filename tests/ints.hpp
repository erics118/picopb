#pragma once

#include <cstdint>
#include <picopb/picopb.hpp>
#include <string_view>
#include <tuple>

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

    static constexpr auto _fields = std::tuple{
        ppb::Field<1, &Ints::i32, ppb::Int32>{},
        ppb::Field<2, &Ints::i64, ppb::Int64>{},
        ppb::Field<3, &Ints::u32, ppb::Uint32>{},
        ppb::Field<4, &Ints::u64, ppb::Uint64>{},
        ppb::Field<5, &Ints::s32, ppb::Sint32>{},
        ppb::Field<6, &Ints::s64, ppb::Sint64>{},
        ppb::Field<7, &Ints::fx32, ppb::Fixed32>{},
        ppb::Field<8, &Ints::fx64, ppb::Fixed64>{},
        ppb::Field<9, &Ints::sfx32, ppb::Sfixed32>{},
        ppb::Field<10, &Ints::sfx64, ppb::Sfixed64>{},
        ppb::Field<11, &Ints::b, ppb::Bool>{},
        ppb::Field<12, &Ints::color, ppb::Enum<Color>>{},
    };

    bool operator==(const Ints&) const = default;
};

}  // namespace picopb::test_messages::ints
