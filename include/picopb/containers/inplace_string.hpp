#pragma once

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <picopb/containers/inplace_vector.hpp>
#include <picopb/utf8.hpp>
#include <string_view>

namespace ppb {

// inplace string implementation
// no terminating \0
// private base so text cant be converted to vector of chars
template <std::size_t N>
class InplaceString : private InplaceVector<char, N> {
    using Base = InplaceVector<char, N>;

   public:
    using typename Base::const_iterator;
    using typename Base::const_pointer;
    using typename Base::const_reference;
    using typename Base::const_reverse_iterator;
    using typename Base::difference_type;
    using typename Base::iterator;
    using typename Base::pointer;
    using typename Base::reference;
    using typename Base::reverse_iterator;
    using typename Base::size_type;
    using typename Base::value_type;

    using Base::assign_range;
    using Base::back;
    using Base::begin;
    using Base::capacity;
    using Base::cbegin;
    using Base::cend;
    using Base::clear;
    using Base::crbegin;
    using Base::crend;
    using Base::data;
    using Base::empty;
    using Base::end;
    using Base::erase;
    using Base::front;
    using Base::max_size;
    using Base::operator[];
    using Base::pop_back;
    using Base::rbegin;
    using Base::rend;
    using Base::resize;
    using Base::shrink_to_fit;
    using Base::size;
    using Base::try_push_back;

    constexpr InplaceString() = default;

    // construct from a string literal
    template <std::size_t M>
    constexpr InplaceString(const char (&literal)[M]) {  // NOLINT(modernize-avoid-c-arrays)
        static_assert(M - 1 <= N, "string literal is longer than the string's capacity");

        (void)resize(M - 1);
        std::ranges::copy(std::string_view{std::data(literal), M - 1}, begin());
    }

    // string_view ctor, keeping as many utf-8 chars that fit in N bytes
    constexpr explicit InplaceString(std::string_view text) {
        const std::size_t size = detail::utf8_prefix(text, N);

        (void)resize(size);
        std::ranges::copy(text.substr(0, size), begin());
    }

    constexpr operator std::string_view() const noexcept { return {data(), size()}; }

    constexpr void swap(InplaceString& other) noexcept { Base::swap(other); }

    friend constexpr void swap(InplaceString& a, InplaceString& b) noexcept { a.swap(b); }

    template <std::size_t M>
    friend constexpr bool operator==(const InplaceString& a, const InplaceString<M>& b) noexcept {
        return std::string_view{a} == std::string_view{b};
    }

    // compare when one is a string literal or std::string or std::string_view
    friend constexpr bool operator==(const InplaceString& a, std::string_view b) noexcept {
        return std::string_view{a} == b;
    }

    template <std::size_t M>
    friend constexpr auto operator<=>(const InplaceString& a, const InplaceString<M>& b) noexcept {
        return std::string_view{a} <=> std::string_view{b};
    }

    friend constexpr auto operator<=>(const InplaceString& a, std::string_view b) noexcept {
        return std::string_view{a} <=> b;
    }
};

// removes every char pred is true for, and returns how many
template <std::size_t N, class Pred>
constexpr std::size_t erase_if(InplaceString<N>& text, Pred pred) {
    const auto removed = std::ranges::remove_if(text, pred);
    const auto count = removed.size();

    text.erase(removed.begin(), removed.end());

    return count;
}

// removes every char equal to c, and returns how many
template <std::size_t N>
constexpr std::size_t erase(InplaceString<N>& text, char c) {
    return erase_if(text, [c](char element) {
        return element == c;
    });
}

}  // namespace ppb
