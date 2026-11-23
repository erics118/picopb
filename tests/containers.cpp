#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <picopb/picopb.hpp>
#include <ranges>
#include <string>
#include <string_view>

namespace {

using Bytes4 = ppb::InplaceBytes<4>;

// the element at position i, since InplaceVector has no operator[]
template <class V>
constexpr auto& at(V& vector, std::ptrdiff_t i) {
    return *std::next(vector.begin(), i);
}

// a vector of size n whose element i is i + 1, so each position is distinguishable
Bytes4 counting(std::size_t n) {
    Bytes4 bytes;

    REQUIRE(bytes.resize(n));

    for (std::size_t i = 0; i < n; ++i) {
        at(bytes, static_cast<std::ptrdiff_t>(i)) = static_cast<std::byte>(i + 1);
    }

    return bytes;
}

// a range, mutable and const, whose begin and end share a type, as range-for and ranges need
static_assert(std::ranges::contiguous_range<Bytes4>);
static_assert(std::ranges::contiguous_range<const Bytes4>);
static_assert(std::ranges::sized_range<Bytes4>);
static_assert(std::ranges::common_range<Bytes4>);
static_assert(std::ranges::common_range<const Bytes4>);
static_assert(std::same_as<std::ranges::range_value_t<Bytes4>, std::byte>);
static_assert(Bytes4::max_size() == 4);

// every operation runs at compile time
static_assert([] {
    Bytes4 a;
    Bytes4 b;
    const bool grew = a.resize(3) && b.resize(3);

    at(a, 1) = std::byte{5};
    at(b, 1) = std::byte{5};

    return grew && a == b && !a.resize(5) && a.size() == 3;
}());

static_assert([] {
    Bytes4 bytes;
    const std::array in{std::byte{1}, std::byte{2}};

    return bytes.assign_range(in) && bytes.size() == 2 && at(bytes, 1) == std::byte{2};
}());

}  // namespace

TEST_CASE("InplaceVector starts empty") {
    const Bytes4 bytes;

    CHECK(bytes.empty());
    CHECK(bytes.size() == 0);
    CHECK(bytes.begin() == bytes.end());
}

TEST_CASE("InplaceVector::resize grows with zeroed elements, up to exactly its capacity") {
    Bytes4 bytes;

    CHECK(bytes.resize(2));
    CHECK(bytes.size() == 2);
    CHECK(std::ranges::equal(bytes, std::array{std::byte{0}, std::byte{0}}));

    CHECK(bytes.resize(4));
    CHECK(bytes.size() == 4);
}

TEST_CASE("InplaceVector::resize past its capacity returns false and changes nothing") {
    Bytes4 bytes = counting(2);

    CHECK_FALSE(bytes.resize(5));
    CHECK(bytes == counting(2));
}

TEST_CASE("InplaceVector::resize shrinks, keeping the leading elements") {
    Bytes4 bytes = counting(4);

    CHECK(bytes.resize(2));
    CHECK(bytes == counting(2));

    CHECK(bytes.resize(0));
    CHECK(bytes.empty());
}

TEST_CASE("InplaceVector zeroes the elements a resize adds back") {
    Bytes4 bytes = counting(3);

    REQUIRE(bytes.resize(1));
    REQUIRE(bytes.resize(3));

    CHECK(std::ranges::equal(bytes, std::array{std::byte{1}, std::byte{0}, std::byte{0}}));
}

TEST_CASE("InplaceVector iterates over exactly its elements, mutable or const") {
    Bytes4 bytes;

    REQUIRE(bytes.resize(3));

    // writing through a mutable range-for reaches every element and stops at the size
    for (std::byte& b : bytes) {
        b = std::byte{1};
    }

    const Bytes4& view = bytes;

    CHECK(std::ranges::equal(view, std::array{std::byte{1}, std::byte{1}, std::byte{1}}));
    CHECK(view.end() - view.begin() == 3);
}

TEST_CASE("InplaceVector::data points at the first element") {
    Bytes4 bytes = counting(2);
    const Bytes4& view = bytes;

    CHECK(bytes.data() == &*bytes.begin());
    CHECK(view.data() == &*view.begin());
}

TEST_CASE("InplaceVector compares only the elements in its size") {
    CHECK(Bytes4{} == Bytes4{});
    CHECK_FALSE(counting(2) == counting(3));

    Bytes4 changed = counting(2);

    at(changed, 1) = std::byte{9};

    CHECK_FALSE(changed == counting(2));

    // an element left behind past the size by a shrink doesn't count
    Bytes4 shrunk = counting(3);

    REQUIRE(shrunk.resize(2));

    CHECK(shrunk == counting(2));
}

TEST_CASE("InplaceVector copies are independent") {
    const Bytes4 original = counting(2);
    Bytes4 copy = original;

    at(copy, 0) = std::byte{9};

    CHECK(original == counting(2));
}

TEST_CASE("InplaceVector holds element types that aren't trivial") {
    ppb::InplaceVector<std::string, 2> names;

    REQUIRE(names.resize(2));
    at(names, 1) = "imu";
    REQUIRE(names.resize(1));
    REQUIRE(names.resize(2));

    CHECK(at(names, 1).empty());
}

TEST_CASE("InplaceVector::assign_range replaces the elements with the range's") {
    Bytes4 bytes = counting(3);
    const std::array in{std::byte{7}, std::byte{8}};

    CHECK(bytes.assign_range(in));
    CHECK(std::ranges::equal(bytes, in));

    CHECK(bytes.assign_range(std::array<std::byte, 0>{}));
    CHECK(bytes.empty());
}

TEST_CASE("InplaceVector::assign_range past its capacity returns false and changes nothing") {
    Bytes4 bytes = counting(2);

    CHECK_FALSE(bytes.assign_range(std::array<std::byte, 5>{}));
    CHECK(bytes == counting(2));
}

TEST_CASE("InplaceString::assign_range takes text up to its capacity") {
    ppb::InplaceString<4> name;

    CHECK(name.assign_range(std::string_view{"imu"}));
    CHECK(name == "imu");

    CHECK_FALSE(name.assign_range(std::string_view{"toolong"}));
    CHECK(name == "imu");
}

TEST_CASE("InplaceString::resize past its capacity returns false and changes nothing") {
    ppb::InplaceString<4> name = "imu";

    CHECK_FALSE(name.resize(5));
    CHECK(name == "imu");

    CHECK(name.resize(4));
    CHECK(name.size() == 4);
}
