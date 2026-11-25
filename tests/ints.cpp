#include "ints.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <picopb/picopb.hpp>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "compare.hpp"
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

using test_util::check_decode;
using test_util::picopb_hex;

// decode runs at compile time: the encoding docs' example, an int32 field 1 set to 150
constexpr std::array one_fifty{std::byte{0x08}, std::byte{0x96}, std::byte{0x01}};

static_assert(ppb::decode<Ints>(one_fifty)->i32 == 150);

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

TEST_CASE("Ints decodes libprotobuf's bytes back to the same message") {
    CHECK(ppb::decode<Ints>({}) == Ints{});
    CHECK(ppb::decode<Ints>(test_util::libprotobuf_encode<Ints>(max_text)) == max_ints);
    CHECK(ppb::decode<Ints>(test_util::libprotobuf_encode<Ints>(min_text)) == min_ints);
}

TEST_CASE("ppb::decode accepts and rejects exactly the bytes libprotobuf does") {
    SUBCASE("valid input") {
        // field 1 twice, so the last value wins
        check_decode<Ints>("08 01 08 02", true);

        // fields 13 to 16 aren't in Ints, one per wire type, so they're skipped
        check_decode<Ints>("68 05", true);
        check_decode<Ints>("72 02 aa bb", true);
        check_decode<Ints>("7d 01 02 03 04", true);
        check_decode<Ints>("81 01 01 02 03 04 05 06 07 08", true);

        // field 1 is an int32 but arrives with the i32 wire type
        check_decode<Ints>("0d 01 02 03 04", true);

        // a bool of 2, and an enum value Color has no name for
        check_decode<Ints>("58 02", true);
        check_decode<Ints>("60 05", true);

        // an int32 of -1 as 10 bytes, and an int32 whose varint is wider than 32 bits
        check_decode<Ints>("08 ff ff ff ff ff ff ff ff ff 01", true);
        check_decode<Ints>("08 80 80 80 80 10", true);

        // a 10-byte varint whose last byte has bits beyond the 64th
        check_decode<Ints>("08 ff ff ff ff ff ff ff ff ff 7f", true);

        // the largest field number, 2^29 - 1, as an unknown field
        check_decode<Ints>("f8 ff ff ff 0f 01", true);
    }

    SUBCASE("malformed input") {
        // a tag with no value, and a varint cut off in the middle
        check_decode<Ints>("08", false);
        check_decode<Ints>("08 80", false);

        // a varint of 11 bytes
        check_decode<Ints>("08 ff ff ff ff ff ff ff ff ff ff 01", false);

        // fixed32, fixed64 and length-delimited values cut short
        check_decode<Ints>("3d 01 02", false);
        check_decode<Ints>("41 01 02 03", false);
        check_decode<Ints>("72 05 aa", false);

        // field number 0
        check_decode<Ints>("00 01", false);

        // wire types 3, 4, 6 and 7, on field 13
        check_decode<Ints>("6b", false);
        check_decode<Ints>("6c", false);
        check_decode<Ints>("6e", false);
        check_decode<Ints>("6f", false);

        // a tag wider than 32 bits
        check_decode<Ints>("80 80 80 80 80 01 01", false);
    }
}

TEST_CASE("ppb::decode reports why the input is malformed") {
    auto error = [](std::string_view hex_bytes) {
        const auto decoded = ppb::decode<Ints>(test_util::from_hex(hex_bytes));

        REQUIRE_FALSE(decoded.has_value());

        return decoded.error();
    };

    CHECK(error("08 80") == ppb::Error::Truncated);
    CHECK(error("72 05 aa") == ppb::Error::Truncated);
    CHECK(error("08 ff ff ff ff ff ff ff ff ff ff 01") == ppb::Error::InvalidVarint);
    CHECK(error("00 01") == ppb::Error::InvalidTag);
    CHECK(error("6e") == ppb::Error::InvalidTag);
}
