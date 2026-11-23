#include "floats.hpp"

#include <doctest/doctest.h>

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <picopb/picopb.hpp>
#include <string_view>

#include "compare.hpp"
#include "libprotobuf.hpp"

namespace {

using picopb::test_messages::floats::Floats;

// whether two messages hold the same bits, since -0.0 == 0.0 and a NaN never equals itself
bool same_bits(const Floats& a, const Floats& b) {
    return std::bit_cast<std::uint64_t>(a.d) == std::bit_cast<std::uint64_t>(b.d) &&
           std::bit_cast<std::uint32_t>(a.f) == std::bit_cast<std::uint32_t>(b.f);
}

// picopb encodes message as libprotobuf encodes text, and decodes those bytes back to message
void check_round_trip(std::string_view text, const Floats& message) {
    INFO(text);

    const auto reference = test_util::libprotobuf_encode<Floats>(text);

    CHECK(test_util::picopb_hex(message) == test_util::hex(reference));

    const auto decoded = ppb::decode<Floats>(reference);

    REQUIRE(decoded.has_value());
    CHECK(same_bits(*decoded, message));
}

// decode runs at compile time: d = 1.5 and f = 1.5
constexpr std::array one_and_a_half{
    std::byte{0x09}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0xf8}, std::byte{0x3f}, std::byte{0x15},
    std::byte{0x00}, std::byte{0x00}, std::byte{0xc0}, std::byte{0x3f},
};

static_assert(ppb::decode<Floats>(one_and_a_half)->d == 1.5);
static_assert(ppb::decode<Floats>(one_and_a_half)->f == 1.5F);

}  // namespace

TEST_CASE("double and float fields encode and decode as libprotobuf does") {
    constexpr double max_double = std::numeric_limits<double>::max();
    constexpr double inf = std::numeric_limits<double>::infinity();
    constexpr float max_float = std::numeric_limits<float>::max();
    constexpr float inf_float = std::numeric_limits<float>::infinity();

    check_round_trip("", Floats{});
    check_round_trip("d: 1.5", Floats{.d = 1.5});
    check_round_trip("d: -0.1", Floats{.d = -0.1});
    check_round_trip("d: 1.7976931348623157e308", Floats{.d = max_double});
    check_round_trip("d: -1.7976931348623157e308", Floats{.d = -max_double});
    check_round_trip("d: 4.9406564584124654e-324",
                     Floats{.d = std::numeric_limits<double>::denorm_min()});
    check_round_trip("d: inf", Floats{.d = inf});
    check_round_trip("d: -inf", Floats{.d = -inf});
    check_round_trip("f: 1.5", Floats{.f = 1.5F});
    check_round_trip("f: -0.1", Floats{.f = -0.1F});
    check_round_trip("f: 3.4028235e38", Floats{.f = max_float});
    check_round_trip("f: inf", Floats{.f = inf_float});
    check_round_trip("d: 1.5 f: -1.5", Floats{.d = 1.5, .f = -1.5F});

    // -0.0 isn't the default, since its sign bit is set, so it's written
    check_round_trip("d: -0", Floats{.d = -0.0});
    check_round_trip("f: -0", Floats{.f = -0.0F});
}

TEST_CASE("a NaN field is written and decoded as a NaN") {
    const Floats nan{.d = std::numeric_limits<double>::quiet_NaN(),
                     .f = std::numeric_limits<float>::quiet_NaN()};

    CHECK(test_util::picopb_hex(nan) ==
          test_util::hex(test_util::libprotobuf_encode<Floats>("d: nan f: nan")));

    const auto decoded =
        ppb::decode<Floats>(test_util::libprotobuf_encode<Floats>("d: nan f: nan"));

    REQUIRE(decoded.has_value());
    CHECK(std::isnan(decoded->d));
    CHECK(std::isnan(decoded->f));
}

TEST_CASE("ppb::decode accepts and rejects exactly the double and float bytes libprotobuf does") {
    using test_util::check_decode;

    check_decode<Floats>("09 00 00 00 00 00 00 f8 3f", true);
    check_decode<Floats>("15 00 00 c0 3f", true);

    // a field with another wire type is skipped like an unknown field
    check_decode<Floats>("08 01", true);
    check_decode<Floats>("10 01", true);

    // cut off inside the value
    check_decode<Floats>("09 00 00 00", false);
    check_decode<Floats>("15 00 00", false);
}
