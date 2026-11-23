#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <picopb/picopb.hpp>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>

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
    const bool wrote = bytes.resize_and_overwrite(2, [](std::byte* data, std::size_t n) {
        std::ranges::fill(std::span(data, n), std::byte{3});

        return n;
    });

    return wrote && bytes.size() == 2 && at(bytes, 1) == std::byte{3};
}());

// whether container.resize_and_overwrite(1, op) is a valid call
template <class C, class Op>
concept Overwritable =
    requires(C container, Op op) { container.resize_and_overwrite(1uz, std::move(op)); };

// an op that can only be moved and only called as an rvalue, which std::string accepts
template <class T>
struct MoveOnlyOp {
    MoveOnlyOp() = default;
    MoveOnlyOp(MoveOnlyOp&&) = default;
    MoveOnlyOp(const MoveOnlyOp&) = delete;

    constexpr std::size_t operator()(T*, std::size_t n) && { return n; }
};

using BytesOp = std::size_t (*)(std::byte*, std::size_t);
using WrongParameters = std::size_t (*)(int);
using WrongReturn = double (*)(std::byte*, std::size_t);

static_assert(Overwritable<Bytes4, BytesOp>);
static_assert(!Overwritable<Bytes4, WrongParameters>);
static_assert(!Overwritable<Bytes4, WrongReturn>);
static_assert(!Overwritable<ppb::InplaceString<4>, WrongParameters>);
static_assert(!Overwritable<ppb::InplaceString<4>, double (*)(char*, std::size_t)>);

static_assert([] {
    Bytes4 bytes;
    ppb::InplaceString<4> name;

    return bytes.resize_and_overwrite(2, MoveOnlyOp<std::byte>{}) &&
           name.resize_and_overwrite(3, MoveOnlyOp<char>{}) && bytes.size() == 2 &&
           name.size() == 3;
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

TEST_CASE("InplaceVector::resize_and_overwrite lets op write the elements, then takes its size") {
    Bytes4 bytes = counting(1);
    const std::byte* buffer = nullptr;
    std::size_t count = 0;

    CHECK(bytes.resize_and_overwrite(4, [&](std::byte* data, std::size_t n) {
        buffer = data;
        count = n;
        std::ranges::fill(std::span(data, n).subspan(1), std::byte{7});

        return 3uz;
    }));

    CHECK(buffer == bytes.data());
    CHECK(count == 4);
    CHECK(std::ranges::equal(bytes, std::array{std::byte{1}, std::byte{7}, std::byte{7}}));
}

TEST_CASE("InplaceVector::resize_and_overwrite past its capacity returns false, skipping op") {
    Bytes4 bytes = counting(2);
    bool called = false;

    CHECK_FALSE(bytes.resize_and_overwrite(5, [&](std::byte*, std::size_t n) {
        called = true;

        return n;
    }));

    CHECK_FALSE(called);
    CHECK(bytes == counting(2));
}

TEST_CASE("InplaceVector::resize_and_overwrite returns false when op returns more than count") {
    Bytes4 bytes = counting(1);

    CHECK_FALSE(bytes.resize_and_overwrite(2, [](std::byte*, std::size_t) {
        return 3uz;
    }));

    CHECK(bytes.size() == 1);
}

TEST_CASE("InplaceString::resize_and_overwrite lets op write the chars, then takes its size") {
    ppb::InplaceString<4> name;

    CHECK(name.resize_and_overwrite(4, [](char* data, std::size_t) {
        std::ranges::copy(std::string_view{"imu"}, data);

        return 3uz;
    }));

    CHECK(name == "imu");
    CHECK_FALSE(name.resize_and_overwrite(5, [](char*, std::size_t n) {
        return n;
    }));
}

TEST_CASE("InplaceString::resize past its capacity returns false and changes nothing") {
    ppb::InplaceString<4> name = "imu";

    CHECK_FALSE(name.resize(5));
    CHECK(name == "imu");

    CHECK(name.resize(4));
    CHECK(name.size() == 4);
}
