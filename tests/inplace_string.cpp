#include <doctest/doctest.h>

#include <compare>
#include <concepts>
#include <iterator>
#include <picopb/picopb.hpp>
#include <string>
#include <string_view>

namespace {

using Text8 = ppb::InplaceString<8>;

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

// the string members run at compile time
static_assert([] {
    Text8 text = "imu";
    const bool pushed = text.try_push_back('x') != nullptr && text.try_push_back('y') != nullptr;

    text.pop_back();

    return pushed && text == "imux" && text < Text8{"imv"} && ppb::erase(text, 'x') == 1;
}());

// text can't be passed or bound as a vector of chars
static_assert(!std::convertible_to<ppb::InplaceString<8>&, ppb::InplaceVector<char, 8>&>);
static_assert(!std::convertible_to<ppb::InplaceString<8>, ppb::InplaceVector<char, 8>>);

// whether t.view() compiles, as a concept so a missing or private view() fails softly
template <class T>
concept HasView = requires(const T& t) { t.view(); };

// text becomes a std::string_view by conversion, as std::string does, with no view() member
static_assert(std::convertible_to<const ppb::InplaceString<8>&, std::string_view>);
static_assert(!HasView<ppb::InplaceString<8>>);

// a literal 0 can't pick erase(const_iterator) as a null pointer
template <class S>
concept ErasesZero = requires(S text) { text.erase(0); };

static_assert(!ErasesZero<Text8>);

}  // namespace

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

TEST_CASE("InplaceString::resize with a char fills only the added chars with it") {
    Text8 text = "imu";

    CHECK(text.resize(5, '!'));
    CHECK(text == "imu!!");

    CHECK_FALSE(text.resize(9, '!'));
    CHECK(text == "imu!!");
}

TEST_CASE("InplaceString::operator[], front and back reach the chars") {
    Text8 text = "imu";
    const Text8& view = text;

    CHECK(text[1] == 'm');
    CHECK(view.front() == 'i');
    CHECK(view.back() == 'u');

    text[0] = 'e';
    text.back() = 's';

    CHECK(text == "ems");
}

TEST_CASE("InplaceString's const and reverse iterators cover exactly its chars") {
    const Text8 text = "imu";

    CHECK(std::string_view(text.cbegin(), text.cend()) == "imu");
    CHECK(std::string(text.rbegin(), text.rend()) == "umi");
    CHECK(std::string(text.crbegin(), text.crend()) == "umi");
}

TEST_CASE("InplaceString::try_push_back appends a char, or is null when full") {
    ppb::InplaceString<4> text = "imu";

    const char* added = text.try_push_back('s');

    REQUIRE(added != nullptr);
    CHECK(added == &text.back());
    CHECK(text == "imus");

    CHECK(text.try_push_back('x') == nullptr);
    CHECK(text == "imus");
}

TEST_CASE("InplaceString::erase, clear and the free erase functions remove chars") {
    Text8 text = "imu imu";

    CHECK(*text.erase(std::next(text.cbegin())) == 'u');
    CHECK(text == "iu imu");

    CHECK(text.erase(text.cbegin(), std::next(text.cbegin(), 3)) == text.begin());
    CHECK(text == "imu");

    CHECK(ppb::erase(text, 'm') == 1);
    CHECK(text == "iu");

    CHECK(ppb::erase_if(text, [](char c) {
              return c == 'u';
          }) == 1);
    CHECK(text == "i");

    text.clear();
    CHECK(text.empty());
}

TEST_CASE("InplaceString::swap exchanges contents, as member and as function") {
    Text8 a = "imu";
    Text8 b = "gps";

    a.swap(b);

    CHECK(a == "gps");
    CHECK(b == "imu");

    swap(a, b);

    CHECK(a == "imu");
    CHECK(b == "gps");
}

TEST_CASE("InplaceString orders as std::string does") {
    const Text8 imu_text = "imu";

    CHECK(imu_text < Text8{"imv"});
    CHECK(imu_text < ppb::InplaceString<4>{"imv"});
    CHECK(imu_text > "im");
    CHECK("imv" > imu_text);
    CHECK(imu_text <= std::string{"imu"});
    CHECK((imu_text <=> std::string_view{"imu"}) == std::strong_ordering::equal);
}
