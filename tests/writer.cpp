#include <doctest/doctest.h>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <format>
#include <initializer_list>
#include <picopb/picopb.hpp>
#include <span>
#include <string_view>
#include <vector>

#include "ints.hpp"
#include "libprotobuf.hpp"
#include "limits.hpp"

namespace {

using picopb::test_messages::ints::Color;
using picopb::test_messages::ints::Ints;

// checks one field of Ints against libprotobuf
void check(std::string_view text, std::invocable<ppb::Writer&> auto write) {
    INFO(text);

    // the expected result
    std::vector<std::byte> expected = test_util::libprotobuf_encode<Ints>(text);

    // ensure that all sizes smaller than it does overflow
    for (std::size_t size = 0; size < expected.size(); ++size) {
        std::vector<std::byte> buffer(size);
        ppb::Writer w{buffer};

        write(w);

        CHECK(w.overflowed());
        CHECK(w.size() == size);
    }

    // alloc a buf with proper size, and it should not overflow and should be exactly equal
    std::vector<std::byte> buffer(expected.size());
    ppb::Writer w{buffer};

    write(w);

    CHECK_FALSE(w.overflowed());
    CHECK(test_util::hex(std::span(buffer).first(w.size())) == test_util::hex(expected));

    // decoding libprotobuf's bytes and encoding the result again gives the same bytes
    auto decoded = ppb::decode<Ints>(expected);

    REQUIRE(decoded.has_value());

    std::vector<std::byte> again(expected.size());
    auto size = ppb::encode(*decoded, again);

    REQUIRE(size.has_value());
    CHECK(test_util::hex(std::span(again).first(*size)) == test_util::hex(expected));
}

// runs check for each value of primitive type S, with the text built from the field's name
template <ppb::Primitive S>
void check_field(std::string_view name, std::uint32_t field_number,
                 std::initializer_list<ppb::cpp_type_t<S>> values) {
    for (ppb::cpp_type_t<S> value : values) {
        check(std::format("{}: {}", name, value), [&](ppb::Writer& w) {
            w.write<S>(field_number, value);
        });
    }
}

}  // namespace

TEST_CASE("each field type matches libprotobuf, overflows shorter buffers, and decodes back") {
    check_field<ppb::Int32>("i32", 1,
                            {0, 1, -1, 127, 128, test_util::int32_min, test_util::int32_max});
    check_field<ppb::Int64>("i64", 2,
                            {0, 1, -1, 127, 128, test_util::int64_min, test_util::int64_max});
    check_field<ppb::Uint32>("u32", 3, {0, 1, 127, 128, test_util::uint32_max});
    check_field<ppb::Uint64>("u64", 4, {0, 1, 127, 128, test_util::uint64_max});

    // zigzag puts the varint's growth from 1 byte to 2 between 63 and 64, and -64 and -65
    check_field<ppb::Sint32>(
        "s32", 5, {0, 1, -1, 63, 64, -64, -65, test_util::int32_min, test_util::int32_max});
    check_field<ppb::Sint64>(
        "s64", 6, {0, 1, -1, 63, 64, -64, -65, test_util::int64_min, test_util::int64_max});

    check_field<ppb::Fixed32>("fx32", 7, {0, 1, test_util::uint32_max});
    check_field<ppb::Fixed64>("fx64", 8, {0, 1, test_util::uint64_max});
    check_field<ppb::Sfixed32>("sfx32", 9, {0, 1, -1, test_util::int32_min, test_util::int32_max});
    check_field<ppb::Sfixed64>("sfx64", 10, {0, 1, -1, test_util::int64_min, test_util::int64_max});
    check_field<ppb::Bool>("b", 11, {false, true});

    // std::format can't print an enum by name, and the text format names enum values
    check("color: COLOR_UNSPECIFIED", [](ppb::Writer& w) {
        w.write<ppb::Enum<Color>>(12, Color::COLOR_UNSPECIFIED);
    });
    check("color: RED", [](ppb::Writer& w) {
        w.write<ppb::Enum<Color>>(12, Color::RED);
    });
    check("color: NEGATIVE", [](ppb::Writer& w) {
        w.write<ppb::Enum<Color>>(12, Color::NEGATIVE);
    });
}
