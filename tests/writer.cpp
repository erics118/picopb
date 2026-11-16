#include <doctest/doctest.h>

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <format>
#include <initializer_list>
#include <limits>
#include <picopb/picopb.hpp>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>

#include "libprotobuf.hpp"

namespace picopb::test::ints {

// stands in for the enum the generator will emit from ints.proto
enum class Color : std::int32_t {
    COLOR_UNSPECIFIED = 0,
    RED = 1,
    NEGATIVE = -1,
};

}  // namespace picopb::test::ints

namespace {

using picopb::test::ints::Color;

constexpr std::int32_t int32_min = std::numeric_limits<std::int32_t>::min();
constexpr std::int32_t int32_max = std::numeric_limits<std::int32_t>::max();
constexpr std::int64_t int64_min = std::numeric_limits<std::int64_t>::min();
constexpr std::int64_t int64_max = std::numeric_limits<std::int64_t>::max();
constexpr std::uint32_t uint32_max = std::numeric_limits<std::uint32_t>::max();
constexpr std::uint64_t uint64_max = std::numeric_limits<std::uint64_t>::max();

// checks one field of picopb.test.int.Ints against libprotobuf
void check(std::string_view text, std::invocable<ppb::Writer&> auto write) {
    INFO(text);

    // the expected result
    std::vector<std::byte> expected = test::libprotobuf_encode("picopb.test.ints.Ints", text);

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
    CHECK(test::hex(std::span(buffer).first(w.size())) == test::hex(expected));
}

// runs check for each value, with the text built from the field's name
// type_identity_t makes T come from the writer, not from the literals in values
template <class T>
void check_field(std::string_view name, void (ppb::Writer::*write_value)(std::uint32_t, T),
                 std::uint32_t field_number,
                 std::initializer_list<std::type_identity_t<T>> values) {
    // check that the write_value function writes the same bytes as libprotobuf
    for (T value : values) {
        check(std::format("{}: {}", name, value),
              [&](ppb::Writer& w) { (w.*write_value)(field_number, value); });
    }
}

// one call per wire type, plus write_enum, so the build fails if any can't run at compile time
constexpr bool encodes_at_compile_time() {
    std::array<std::byte, 32> buffer{};
    ppb::Writer w{buffer};

    w.write_int32(1, -1);
    w.write_fixed32(7, 1);
    w.write_fixed64(8, 1);
    w.write_enum(12, Color::RED);

    // 11 + 5 + 9 + 2
    return w.size() == 27 && !w.overflowed();
}

static_assert(encodes_at_compile_time());

}  // namespace

TEST_CASE("each field writer gives libprotobuf's bytes, and overflows every shorter buffer") {
    check_field("i32", &ppb::Writer::write_int32, 1, {0, 1, -1, 127, 128, int32_min, int32_max});
    check_field("i64", &ppb::Writer::write_int64, 2, {0, 1, -1, 127, 128, int64_min, int64_max});
    check_field("u32", &ppb::Writer::write_uint32, 3, {0, 1, 127, 128, uint32_max});
    check_field("u64", &ppb::Writer::write_uint64, 4, {0, 1, 127, 128, uint64_max});

    // zigzag puts the varint's growth from 1 byte to 2 between 63 and 64, and -64 and -65
    check_field("s32", &ppb::Writer::write_sint32, 5,
                {0, 1, -1, 63, 64, -64, -65, int32_min, int32_max});
    check_field("s64", &ppb::Writer::write_sint64, 6,
                {0, 1, -1, 63, 64, -64, -65, int64_min, int64_max});

    check_field("fx32", &ppb::Writer::write_fixed32, 7, {0, 1, uint32_max});
    check_field("fx64", &ppb::Writer::write_fixed64, 8, {0, 1, uint64_max});
    check_field("sfx32", &ppb::Writer::write_sfixed32, 9, {0, 1, -1, int32_min, int32_max});
    check_field("sfx64", &ppb::Writer::write_sfixed64, 10, {0, 1, -1, int64_min, int64_max});
    check_field("b", &ppb::Writer::write_bool, 11, {false, true});

    // std::format can't print an enum by name, and the text format names enum values
    check("color: COLOR_UNSPECIFIED",
          [](ppb::Writer& w) { w.write_enum(12, Color::COLOR_UNSPECIFIED); });
    check("color: RED", [](ppb::Writer& w) { w.write_enum(12, Color::RED); });
    check("color: NEGATIVE", [](ppb::Writer& w) { w.write_enum(12, Color::NEGATIVE); });
}
