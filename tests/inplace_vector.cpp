#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <compare>
#include <cstddef>
#include <iterator>
#include <picopb/picopb.hpp>
#include <ranges>
#include <string>
#include <utility>

namespace {

using Bytes4 = ppb::InplaceBytes<4>;

// a vector of size n whose element i is i + 1, so each position is distinguishable
Bytes4 counting(std::size_t n) {
    Bytes4 bytes;

    REQUIRE(bytes.resize(n));

    for (std::size_t i = 0; i < n; ++i) {
        bytes[i] = static_cast<std::byte>(i + 1);
    }

    return bytes;
}

// a range, mutable and const, whose begin and end share a type, as range-for and ranges need
static_assert(std::ranges::contiguous_range<Bytes4>);
static_assert(std::ranges::contiguous_range<const Bytes4>);
static_assert(std::ranges::sized_range<Bytes4>);
static_assert(std::ranges::common_range<Bytes4>);
static_assert(std::ranges::common_range<const Bytes4>);
static_assert(Bytes4::max_size() == 4);
static_assert(Bytes4::capacity() == 4);

// every operation runs at compile time
static_assert([] {
    Bytes4 a;
    Bytes4 b;
    const bool grew = a.resize(3) && b.resize(3);

    a[1] = std::byte{5};
    b[1] = std::byte{5};

    return grew && a == b && !a.resize(5) && a.size() == 3;
}());

static_assert([] {
    Bytes4 bytes;
    const std::array in{std::byte{1}, std::byte{2}};

    return bytes.assign_range(in) && bytes.size() == 2 && bytes[1] == std::byte{2};
}());

// the appends, removals, swap and ordering run at compile time
static_assert([] {
    Bytes4 a;
    Bytes4 b;
    const bool pushed =
        a.try_push_back(std::byte{1}) != nullptr && a.try_emplace_back(std::byte{2}) != nullptr &&
        a.try_push_back(std::byte{3}) != nullptr && b.try_emplace_back(std::byte{9}) != nullptr;
    a.pop_back();
    a.swap(b);
    auto* const rest = a.erase(a.begin());
    const bool filled = b.resize(4, std::byte{7}) && b[3] == std::byte{7};

    return pushed && filled && a.empty() && rest == a.end() && a < b &&
           ppb::erase(b, std::byte{7}) == 2;
}());

// every slot holds a live T, so T must be default-constructible and movable
template <class T>
concept HoldsElements = requires { typename ppb::InplaceVector<T, 1>; };

struct NoDefault {
    explicit NoDefault(int /*value*/) {}
};

struct NoMove {
    NoMove() = default;
    NoMove(const NoMove&) = delete;
    NoMove(NoMove&&) = delete;
    NoMove& operator=(const NoMove&) = delete;
    NoMove& operator=(NoMove&&) = delete;
    ~NoMove() = default;
};

static_assert(!HoldsElements<NoDefault>);
static_assert(!HoldsElements<NoMove>);

// emplacing constructs T from the arguments, so it can't cast away const as T(arg) would
template <class V, class Arg>
concept EmplacesFrom = requires(V vector, Arg arg) { vector.try_emplace_back(arg); };

static_assert(!EmplacesFrom<ppb::InplaceVector<char*, 2>, const char*>);

// a literal 0 can't pick erase(const_iterator) as a null pointer
template <class V>
concept ErasesZero = requires(V vector) { vector.erase(0); };

static_assert(!ErasesZero<Bytes4>);

// has no ordering at all
struct NoOrder {};

template <class V>
concept HasSpaceship = requires(const V& a, const V& b) { a <=> b; };

// a vector orders only when its elements do, as std's containers
static_assert(!HasSpaceship<ppb::InplaceVector<NoOrder, 1>>);

// counts the times an element is made
int& defaults_made() {
    static int count = 0;
    return count;
}

struct CountsDefault {
    CountsDefault() { ++defaults_made(); }
};

// can be moved but not copied, so only an rvalue overload accepts it
struct MoveOnly {
    int value = 0;

    MoveOnly() = default;

    explicit MoveOnly(int v) : value(v) {}

    MoveOnly(const MoveOnly&) = delete;
    MoveOnly(MoveOnly&&) = default;
    MoveOnly& operator=(const MoveOnly&) = delete;
    MoveOnly& operator=(MoveOnly&&) = default;
    ~MoveOnly() = default;
};

}  // namespace

TEST_CASE("InplaceVector starts empty") {
    const Bytes4 bytes;

    CHECK(bytes.empty());
    CHECK(bytes.size() == 0);
    CHECK(bytes.begin() == bytes.end());
}

TEST_CASE("InplaceVector copies are independent") {
    const Bytes4 original = counting(2);
    Bytes4 copy = original;

    copy[0] = std::byte{9};

    CHECK(original == counting(2));
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

TEST_CASE("InplaceVector holds element types that aren't trivial") {
    ppb::InplaceVector<std::string, 2> names;

    REQUIRE(names.resize(2));
    names[1] = "imu";
    REQUIRE(names.resize(1));
    REQUIRE(names.resize(2));

    CHECK(names[1].empty());
}

TEST_CASE("InplaceVector::resize with a value fills only the added elements with it") {
    Bytes4 bytes = counting(2);

    CHECK(bytes.resize(4, std::byte{7}));
    CHECK(std::ranges::equal(bytes,
                             std::array{std::byte{1}, std::byte{2}, std::byte{7}, std::byte{7}}));

    CHECK(bytes.resize(1, std::byte{7}));
    CHECK(bytes == counting(1));

    CHECK_FALSE(bytes.resize(5, std::byte{7}));
    CHECK(bytes == counting(1));
}

TEST_CASE("InplaceVector::resize past its capacity makes no element") {
    ppb::InplaceVector<CountsDefault, 2> v;

    defaults_made() = 0;

    CHECK_FALSE(v.resize(3));
    CHECK(defaults_made() == 0);
}

TEST_CASE("InplaceVector::resize makes an element only for each one it adds") {
    ppb::InplaceVector<CountsDefault, 2> v;

    REQUIRE(v.resize(2));

    defaults_made() = 0;

    CHECK(v.resize(2));
    CHECK(v.resize(1));
    CHECK(v.resize(0));
    CHECK(defaults_made() == 0);
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

TEST_CASE("InplaceVector::data points at the first element") {
    Bytes4 bytes = counting(2);
    const Bytes4& view = bytes;

    CHECK(bytes.data() == &*bytes.begin());
    CHECK(view.data() == &*view.begin());
}

TEST_CASE("InplaceVector::operator[], front and back reach the elements") {
    Bytes4 bytes = counting(3);
    const Bytes4& view = bytes;

    CHECK(bytes[0] == std::byte{1});
    CHECK(view[2] == std::byte{3});
    CHECK(bytes.front() == std::byte{1});
    CHECK(view.back() == std::byte{3});

    bytes[1] = std::byte{7};
    bytes.front() = std::byte{8};
    bytes.back() = std::byte{9};

    CHECK(std::ranges::equal(bytes, std::array{std::byte{8}, std::byte{7}, std::byte{9}}));
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

TEST_CASE("InplaceVector's const and reverse iterators cover exactly its elements") {
    const Bytes4 bytes = counting(3);
    const std::array reversed{std::byte{3}, std::byte{2}, std::byte{1}};

    CHECK(std::ranges::equal(std::ranges::subrange(bytes.cbegin(), bytes.cend()), bytes));
    CHECK(std::ranges::equal(std::ranges::subrange(bytes.rbegin(), bytes.rend()), reversed));
    CHECK(std::ranges::equal(std::ranges::subrange(bytes.crbegin(), bytes.crend()), reversed));

    Bytes4 mutable_bytes = counting(2);
    *mutable_bytes.rbegin() = std::byte{9};

    CHECK(mutable_bytes[1] == std::byte{9});
    CHECK(mutable_bytes.begin() != mutable_bytes.cend());
}

TEST_CASE(
    "InplaceVector::try_push_back appends and points at the new element, or is null when "
    "full") {
    Bytes4 bytes = counting(3);

    const std::byte* added = bytes.try_push_back(std::byte{9});

    REQUIRE(added != nullptr);
    CHECK(added == &bytes.back());
    CHECK(bytes.size() == 4);

    CHECK(bytes.try_push_back(std::byte{5}) == nullptr);
    CHECK(bytes.size() == 4);
    CHECK(bytes.back() == std::byte{9});
}

TEST_CASE("InplaceVector::try_push_back moves an rvalue in") {
    ppb::InplaceVector<MoveOnly, 1> v;
    MoveOnly four{4};

    REQUIRE(v.try_push_back(std::move(four)) != nullptr);
    CHECK(v[0].value == 4);
}

TEST_CASE("InplaceVector::try_emplace_back constructs from the arguments") {
    ppb::InplaceVector<std::string, 1> names;
    const std::string* built = names.try_emplace_back(3uz, 'a');

    REQUIRE(built != nullptr);
    CHECK(*built == "aaa");
    CHECK(names.try_emplace_back(1uz, 'b') == nullptr);
}

TEST_CASE("InplaceVector::pop_back and clear remove elements") {
    Bytes4 bytes = counting(3);

    bytes.pop_back();

    CHECK(bytes == counting(2));

    bytes.clear();

    CHECK(bytes.empty());
}

TEST_CASE("InplaceVector::erase removes one element or a range, returning what follows") {
    SUBCASE("one element") {
        Bytes4 bytes = counting(4);
        auto* const next = bytes.erase(std::next(bytes.cbegin()));

        CHECK(std::ranges::equal(bytes, std::array{std::byte{1}, std::byte{3}, std::byte{4}}));
        CHECK(*next == std::byte{3});
    }

    SUBCASE("the last element") {
        Bytes4 bytes = counting(2);
        auto* const next = bytes.erase(std::next(bytes.cbegin()));

        CHECK(next == bytes.end());
        CHECK(bytes == counting(1));
    }

    SUBCASE("a range") {
        Bytes4 bytes = counting(4);
        auto* const next = bytes.erase(std::next(bytes.cbegin()), std::next(bytes.cbegin(), 3));

        CHECK(std::ranges::equal(bytes, std::array{std::byte{1}, std::byte{4}}));
        CHECK(*next == std::byte{4});
    }

    SUBCASE("an empty range changes nothing") {
        Bytes4 bytes = counting(3);
        auto* const next = bytes.erase(std::next(bytes.cbegin()), std::next(bytes.cbegin()));

        CHECK(bytes == counting(3));
        CHECK(next == std::next(bytes.begin()));
    }
}

TEST_CASE("ppb::erase and ppb::erase_if remove the matches and count them") {
    Bytes4 bytes = counting(4);

    CHECK(ppb::erase(bytes, std::byte{2}) == 1);
    CHECK(std::ranges::equal(bytes, std::array{std::byte{1}, std::byte{3}, std::byte{4}}));

    CHECK(ppb::erase_if(bytes, [](std::byte b) {
              return b > std::byte{2};
          }) == 2);
    CHECK(bytes == counting(1));
}

TEST_CASE("InplaceVector::swap exchanges contents and sizes, as member and as function") {
    Bytes4 a = counting(1);
    Bytes4 b = counting(3);

    a.swap(b);

    CHECK(a == counting(3));
    CHECK(b == counting(1));

    swap(a, b);

    CHECK(a == counting(1));
    CHECK(b == counting(3));
}

TEST_CASE("InplaceVector::swap with itself changes nothing") {
    Bytes4 bytes = counting(3);

    bytes.swap(bytes);

    CHECK(bytes == counting(3));
}

TEST_CASE("InplaceVector compares only the elements in its size") {
    CHECK(Bytes4{} == Bytes4{});
    CHECK_FALSE(counting(2) == counting(3));

    Bytes4 changed = counting(2);

    changed[1] = std::byte{9};

    CHECK_FALSE(changed == counting(2));

    // an element left behind past the size by a shrink doesn't count
    Bytes4 shrunk = counting(3);

    REQUIRE(shrunk.resize(2));

    CHECK(shrunk == counting(2));
}

TEST_CASE("InplaceVector orders lexicographically") {
    CHECK(counting(2) < counting(3));
    CHECK(counting(3) > counting(2));

    Bytes4 bigger = counting(2);
    bigger[1] = std::byte{9};

    CHECK(counting(3) < bigger);
    CHECK((counting(2) <=> counting(2)) == std::strong_ordering::equal);
}
