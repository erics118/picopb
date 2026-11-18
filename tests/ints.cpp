#include "ints.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <picopb/picopb.hpp>
#include <span>
#include <string_view>
#include <vector>

#include "libprotobuf.hpp"
#include "limits.hpp"

namespace {

using picopb::test_messages::ints::Color;
using picopb::test_messages::ints::Ints;

constexpr Ints max_ints{
    .i32 = test_util::int32_max,
    .i64 = test_util::int64_max,
    .u32 = test_util::uint32_max,
    .u64 = test_util::uint64_max,
    .s32 = test_util::int32_max,
    .s64 = test_util::int64_max,
    .fx32 = test_util::uint32_max,
    .fx64 = test_util::uint64_max,
    .sfx32 = test_util::int32_max,
    .sfx64 = test_util::int64_max,
    .b = true,
    .color = Color::RED,
};

constexpr std::string_view max_text =
    "i32: 2147483647 i64: 9223372036854775807 u32: 4294967295 u64: 18446744073709551615 "
    "s32: 2147483647 s64: 9223372036854775807 fx32: 4294967295 fx64: 18446744073709551615 "
    "sfx32: 2147483647 sfx64: 9223372036854775807 b: true color: RED";

// the unsigned fields have no minimum below 0, so they stay at their default
constexpr Ints min_ints{
    .i32 = test_util::int32_min,
    .i64 = test_util::int64_min,
    .s32 = test_util::int32_min,
    .s64 = test_util::int64_min,
    .sfx32 = test_util::int32_min,
    .sfx64 = test_util::int64_min,
    .color = Color::NEGATIVE,
};

constexpr std::string_view min_text =
    "i32: -2147483648 i64: -9223372036854775808 s32: -2147483648 s64: -9223372036854775808 "
    "sfx32: -2147483648 sfx64: -9223372036854775808 color: NEGATIVE";

// the bytes ppb::encode writes for message, as hex text
std::string picopb_hex(const Ints& message) {
    std::vector<std::byte> buffer(128);
    auto size = ppb::encode(message, buffer);

    REQUIRE(size.has_value());

    return test_util::hex(std::span(buffer).first(*size));
}

}  // namespace

TEST_CASE("Ints encodes to libprotobuf's bytes") {
    SUBCASE("every field at its default writes nothing") { CHECK(picopb_hex(Ints{}).empty()); }

    SUBCASE("every field at its maximum") {
        CHECK(picopb_hex(max_ints) ==
              test_util::hex(test_util::libprotobuf_encode<Ints>(max_text)));
    }

    SUBCASE("every signed field at its minimum") {
        CHECK(picopb_hex(min_ints) ==
              test_util::hex(test_util::libprotobuf_encode<Ints>(min_text)));
    }

    SUBCASE("a few small values, written in field number order") {
        constexpr Ints small{.u32 = 1, .s64 = -1, .fx32 = 7, .b = true};

        CHECK(picopb_hex(small) == test_util::hex(test_util::libprotobuf_encode<Ints>(
                                       "u32: 1 s64: -1 fx32: 7 b: true")));
    }
}

TEST_CASE("ppb::encode gives BufferTooSmall for every buffer shorter than the encoding") {
    const std::size_t size = test_util::libprotobuf_encode<Ints>(max_text).size();

    for (std::size_t short_size = 0; short_size < size; ++short_size) {
        std::vector<std::byte> buffer(short_size);
        auto result = ppb::encode(max_ints, buffer);

        INFO("buffer size ", short_size);

        REQUIRE_FALSE(result.has_value());
        CHECK(result.error() == ppb::Error::BufferTooSmall);
    }

    std::vector<std::byte> exact(size);
    auto result = ppb::encode(max_ints, exact);

    REQUIRE(result.has_value());
    CHECK(*result == size);
}
