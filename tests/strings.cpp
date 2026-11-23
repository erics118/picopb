#include "strings.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <picopb/picopb.hpp>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "compare.hpp"
#include "libprotobuf.hpp"

namespace {

using picopb::test_messages::strings::InplaceStrings;
using picopb::test_messages::strings::StdStrings;

// eight ascii bytes, the size of the utf-8 check's fast path
constexpr std::string_view ascii8 = "61 61 61 61 61 61 61 61";

std::string with_ascii8(std::string_view before, std::string_view after) {
    return std::string(before) + std::string(ascii8) + std::string(after);
}

// field 1 holding 17 bytes, one more than InplaceStrings can hold
std::string seventeen_bytes() {
    std::string out = "0a 11";

    for (int i = 0; i < 17; ++i) {
        out += " 61";
    }

    return out;
}

// a literal fits, and InplaceString works at compile time
constexpr ppb::InplaceString<8> imu = "imu";

static_assert(imu == std::string_view{"imu"});
static_assert(imu.size() == 3);

// every way to compare text, in both orders, with no ambiguity
constexpr const char* imu_pointer = "imu";

static_assert(imu == ppb::InplaceString<8>{"imu"});
static_assert(imu == ppb::InplaceString<4>{"imu"} && ppb::InplaceString<4>{"imu"} == imu);
static_assert(imu != ppb::InplaceString<4>{"imx"});
static_assert(imu == "imu" && "imu" == imu && imu != "imx");
static_assert(imu != "a literal longer than eight");
static_assert(imu == imu_pointer && imu_pointer == imu);
static_assert(std::string_view{"imu"} == imu && imu != std::string_view{"im"});
static_assert(imu == std::string{"imu"} && std::string{"imu"} == imu);

// a continuation byte is 10xxxxxx
static_assert(ppb::detail::is_continuation(0x80));
static_assert(ppb::detail::is_continuation(0xbf));
static_assert(!ppb::detail::is_continuation(0xc0));
static_assert(!ppb::detail::is_continuation(0x7f));

// the longest prefix of at most max bytes that doesn't split a character
static_assert(ppb::detail::utf8_prefix("abc", 2) == 2);
static_assert(ppb::detail::utf8_prefix("abc", 5) == 3);
static_assert(ppb::detail::utf8_prefix("ab\xc3\xa9", 3) == 2);  // é is c3 a9
static_assert(ppb::detail::utf8_prefix("ab\xc3\xa9", 4) == 4);
static_assert(ppb::detail::utf8_prefix("\xf0\x9f\x98\x80", 3) == 0);

}  // namespace

TEST_CASE_TEMPLATE("string and bytes fields decode and encode as libprotobuf does", M,
                   InplaceStrings, StdStrings) {
    using test_util::check_decode;

    SUBCASE("valid input") {
        check_decode<M>("0a 03 69 6d 75", true);
        check_decode<M>("12 03 00 01 ff", true);
        check_decode<M>("0a 03 69 6d 75 12 02 00 ff", true);

        // empty values decode to the default, and encode to nothing
        check_decode<M>("0a 00", true);
        check_decode<M>("12 00", true);

        // two, three and four byte characters, and a zero byte inside a string
        check_decode<M>("0a 02 c3 a9", true);
        check_decode<M>("0a 03 e2 82 ac", true);
        check_decode<M>("0a 04 f0 9f 98 80", true);
        check_decode<M>("0a 03 61 00 62", true);

        // field 1 twice, so the last value wins
        check_decode<M>("0a 01 61 0a 01 62", true);

        // the fast path's edges: a character after 8 ascii bytes, across the 8th byte, before 8
        check_decode<M>(with_ascii8("0a 0a ", " c3 a9"), true);
        check_decode<M>("0a 09 61 61 61 61 61 61 61 c3 a9", true);
        check_decode<M>(with_ascii8("0a 0a c3 a9 ", ""), true);
        check_decode<M>("0a 10 " + std::string(ascii8) + " " + std::string(ascii8), true);

        // field 1 is a string but arrives as a varint, so it's skipped like an unknown field
        check_decode<M>("08 01", true);

        // bytes that aren't utf-8 are fine in a bytes field
        check_decode<M>("12 02 c3 28", true);
        check_decode<M>("12 01 ff", true);
    }

    SUBCASE("invalid utf-8 in a string field") {
        // a lead byte followed by '(' instead of a continuation byte
        check_decode<M>("0a 02 c3 28", false);

        // a continuation byte with no lead, and a byte that can't start a character
        check_decode<M>("0a 01 80", false);
        check_decode<M>("0a 01 ff", false);

        // a two-byte character cut off
        check_decode<M>("0a 01 c3", false);

        // 0 written in two bytes, a utf-16 surrogate, and U+110000
        check_decode<M>("0a 02 c0 80", false);
        check_decode<M>("0a 03 ed a0 80", false);
        check_decode<M>("0a 04 f4 90 80 80", false);

        // a bad character after the fast path skips 8 ascii bytes, and after it starts again
        check_decode<M>(with_ascii8("0a 0a ", " c3 28"), false);
        check_decode<M>(with_ascii8("0a 0b c3 a9 ", " ff"), false);
    }

    SUBCASE("cut-off input") {
        check_decode<M>("0a", false);
        check_decode<M>("0a 05 61", false);
    }
}

TEST_CASE_TEMPLATE("decoded strings and bytes hold the field's contents", M, InplaceStrings,
                   StdStrings) {
    const auto message = ppb::decode<M>(test_util::from_hex("0a 03 69 6d 75 12 02 00 ff"));

    REQUIRE(message.has_value());
    CHECK(message->s == std::string_view{"imu"});
    CHECK(std::ranges::equal(message->b, std::array{std::byte{0x00}, std::byte{0xff}}));
}

TEST_CASE_TEMPLATE("empty strings and bytes aren't written", M, InplaceStrings, StdStrings) {
    CHECK(test_util::picopb_hex(M{}).empty());
}

TEST_CASE_TEMPLATE("decode reports why a string field is malformed", M, InplaceStrings,
                   StdStrings) {
    auto error = [](std::string_view hex_bytes) {
        const auto decoded = ppb::decode<M>(test_util::from_hex(hex_bytes));

        REQUIRE_FALSE(decoded.has_value());

        return decoded.error();
    };

    CHECK(error("0a 02 c3 28") == ppb::Error::InvalidUtf8);
    CHECK(error("0a 05 61") == ppb::Error::Truncated);
}

TEST_CASE("only the fixed-capacity storage rejects a value longer than its capacity") {
    const std::vector<std::byte> bytes = test_util::from_hex(seventeen_bytes());

    // libprotobuf and std::string have no limit
    REQUIRE(test_util::libprotobuf_reencode<StdStrings>(bytes).has_value());
    REQUIRE(ppb::decode<StdStrings>(bytes).has_value());
    CHECK(ppb::decode<StdStrings>(bytes)->s.size() == 17);

    const auto inplace = ppb::decode<InplaceStrings>(bytes);

    REQUIRE_FALSE(inplace.has_value());
    CHECK(inplace.error() == ppb::Error::CapacityExceeded);
}

TEST_CASE("InplaceString converts to std::string, and from it by cutting at its capacity") {
    SUBCASE("to std::string") {
        const ppb::InplaceString<8> name = "imu";
        const std::string text(name);

        CHECK(text == "imu");
    }

    SUBCASE("from a string that fits") {
        const ppb::InplaceString<8> name{std::string("imu")};

        CHECK(name == "imu");
    }

    SUBCASE("from a longer string, keeping the first N bytes") {
        const ppb::InplaceString<4> name{std::string("abcdef")};

        CHECK(name == "abcd");
    }

    SUBCASE("never splitting a utf-8 character") {
        // "abé", where é is the two bytes c3 a9
        const std::string text = "ab\xc3\xa9";

        CHECK(ppb::InplaceString<4>{text} == text);
        CHECK(ppb::InplaceString<3>{text} == "ab");
        CHECK(ppb::InplaceString<2>{text} == "ab");

        // a four-byte character that doesn't fit at all
        CHECK(ppb::InplaceString<3>{std::string("\xf0\x9f\x98\x80")}.empty());
    }
}
