#include <doctest/doctest.h>

#include <algorithm>
#include <array>
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
void check(std::string_view text, std::invocable<ppb::detail::Writer&> auto write) {
    INFO(text);

    // the expected result
    std::vector<std::byte> expected = test_util::libprotobuf_encode<Ints>(text);

    // ensure that all sizes smaller than it does overflow
    for (std::size_t size = 0; size < expected.size(); ++size) {
        std::vector<std::byte> buffer(size);
        ppb::detail::Writer w{buffer};

        write(w);

        CHECK(w.overflowed());
        CHECK(w.size() == size);
    }

    // alloc a buf with proper size, and it should not overflow and should be exactly equal
    std::vector<std::byte> buffer(expected.size());
    ppb::detail::Writer w{buffer};

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

// runs check for each value of one Ints field, written by its table entry
// the entry skips a 0, so the 0 cases check implicit presence
template <std::uint32_t N, auto Member, class S>
void check_field(std::string_view name, std::initializer_list<typename S::cpp_type> values) {
    for (typename S::cpp_type value : values) {
        Ints message{};
        message.*Member = value;

        check(std::format("{}: {}", name, value), [&](ppb::detail::Writer& w) {
            REQUIRE(ppb::Field<N, Member, S>::write(w, message).has_value());
        });
    }
}

// std::format can't print an enum by name, and the text format names enum values
void check_color(std::string_view text, Color color) {
    check(text, [&](ppb::detail::Writer& w) {
        REQUIRE(ppb::Field<12, &Ints::color, Color>::write(w, Ints{.color = color}).has_value());
    });
}

}  // namespace

TEST_CASE("each field type matches libprotobuf, overflows shorter buffers, and decodes back") {
    check_field<1, &Ints::i32, ppb::Int32>(
        "i32", {0, 1, -1, 127, 128, test_util::int32_min, test_util::int32_max});
    check_field<2, &Ints::i64, ppb::Int64>(
        "i64", {0, 1, -1, 127, 128, test_util::int64_min, test_util::int64_max});
    check_field<3, &Ints::u32, ppb::Uint32>("u32", {0, 1, 127, 128, test_util::uint32_max});
    check_field<4, &Ints::u64, ppb::Uint64>("u64", {0, 1, 127, 128, test_util::uint64_max});

    // zigzag puts the varint's growth from 1 byte to 2 between 63 and 64, and -64 and -65
    check_field<5, &Ints::s32, ppb::Sint32>(
        "s32", {0, 1, -1, 63, 64, -64, -65, test_util::int32_min, test_util::int32_max});
    check_field<6, &Ints::s64, ppb::Sint64>(
        "s64", {0, 1, -1, 63, 64, -64, -65, test_util::int64_min, test_util::int64_max});

    check_field<7, &Ints::fx32, ppb::Fixed32>("fx32", {0, 1, test_util::uint32_max});
    check_field<8, &Ints::fx64, ppb::Fixed64>("fx64", {0, 1, test_util::uint64_max});
    check_field<9, &Ints::sfx32, ppb::Sfixed32>(
        "sfx32", {0, 1, -1, test_util::int32_min, test_util::int32_max});
    check_field<10, &Ints::sfx64, ppb::Sfixed64>(
        "sfx64", {0, 1, -1, test_util::int64_min, test_util::int64_max});
    check_field<11, &Ints::b, ppb::Bool>("b", {false, true});

    check_color("color: COLOR_UNSPECIFIED", Color::COLOR_UNSPECIFIED);
    check_color("color: RED", Color::RED);
    check_color("color: NEGATIVE", Color::NEGATIVE);
}

TEST_CASE("Writer writes a 0 varint, since skipping a 0 is the field type's job") {
    std::array<std::byte, 2> buffer{};
    ppb::detail::Writer w{buffer};

    w.write_tag(1, ppb::detail::WireType::Varint);
    w.write_varint(0);

    CHECK_FALSE(w.overflowed());
    CHECK(test_util::hex(std::span(buffer).first(w.size())) == "08 00");
}

TEST_CASE("Writer writes each wire value, and Reader reads it back") {
    std::array<std::byte, 32> buffer{};
    ppb::detail::Writer w{buffer};
    const std::array payload{std::byte{0x61}, std::byte{0x62}};

    w.write_tag(1, ppb::detail::WireType::Varint);
    w.write_varint(300);
    w.write_tag(2, ppb::detail::WireType::I32);
    w.write_i32(0x01020304);
    w.write_tag(3, ppb::detail::WireType::I64);
    w.write_i64(0x0102030405060708);
    w.write_tag(4, ppb::detail::WireType::Len);
    w.write_len(std::span<const std::byte>(payload));

    REQUIRE_FALSE(w.overflowed());
    CHECK(test_util::hex(std::span(buffer).first(w.size())) ==
          "08 ac 02 15 04 03 02 01 19 08 07 06 05 04 03 02 01 22 02 61 62");

    ppb::detail::Reader r{std::span<const std::byte>(buffer).first(w.size())};

    // the next tag, failing the test if there isn't one
    const auto next_tag = [&r] {
        const auto tag = r.read_tag();

        REQUIRE(tag.has_value());

        return *tag;
    };

    CHECK(next_tag().wire == ppb::detail::WireType::Varint);
    CHECK(r.read_varint() == 300U);
    CHECK(next_tag().wire == ppb::detail::WireType::I32);
    CHECK(r.read_i32() == 0x01020304U);
    CHECK(next_tag().wire == ppb::detail::WireType::I64);
    CHECK(r.read_i64() == 0x0102030405060708U);
    CHECK(next_tag().field_number == 4);

    const auto bytes = r.read_len();

    REQUIRE(bytes.has_value());
    CHECK(std::ranges::equal(*bytes, payload));
    CHECK(r.done());
}

TEST_CASE("Reader reports a wire value cut off by the end of input") {
    const std::vector<std::byte> i32 = test_util::from_hex("04 03");
    const std::vector<std::byte> len = test_util::from_hex("05 61");

    const auto cut_i32 = ppb::detail::Reader{i32}.read_i32();
    const auto cut_len = ppb::detail::Reader{len}.read_len();

    REQUIRE_FALSE(cut_i32.has_value());
    REQUIRE_FALSE(cut_len.has_value());
    CHECK(cut_i32.error() == ppb::Error::Truncated);
    CHECK(cut_len.error() == ppb::Error::Truncated);
}
