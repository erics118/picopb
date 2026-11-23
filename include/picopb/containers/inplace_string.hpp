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
    using typename Base::value_type;

    using Base::begin;
    using Base::data;
    using Base::empty;
    using Base::end;
    using Base::max_size;
    using Base::resize;
    using Base::resize_and_overwrite;
    using Base::size;

    constexpr InplaceString() = default;

    // construct from a string literal
    template <std::size_t M>
    constexpr InplaceString(const char (&literal)[M]) {  // NOLINT(modernize-avoid-c-arrays)
        static_assert(M - 1 <= N, "string literal longer than the string's capacity");

        (void)resize(M - 1);
        std::ranges::copy(std::string_view{std::data(literal), M - 1}, begin());
    }

    // string_view ctor, keeping as many utf-8 chars that fit in N bytes
    constexpr explicit InplaceString(std::string_view text) {
        const std::size_t size = detail::utf8_prefix(text, N);

        (void)resize(size);
        std::ranges::copy(text.substr(0, size), begin());
    }

    constexpr operator std::string_view() const { return {data(), size()}; }

    template <std::size_t M>
    friend constexpr bool operator==(const InplaceString& a, const InplaceString<M>& b) {
        return std::string_view{a} == std::string_view{b};
    }

    // compare when one is a string literal or std::string or std::string_view
    friend constexpr bool operator==(const InplaceString& a, std::string_view b) {
        return std::string_view{a} == b;
    }
};

}  // namespace ppb
