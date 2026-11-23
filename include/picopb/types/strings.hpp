#pragma once

#include <concepts>
#include <cstddef>
#include <ranges>
#include <span>

namespace ppb {

namespace detail {

// the shared shape a string container or byte container has
template <class C>
concept ByteContainer =
    std::ranges::contiguous_range<C> && std::ranges::sized_range<C> &&
    sizeof(std::ranges::range_value_t<C>) == 1 &&
    requires(C& container, std::span<const std::ranges::range_value_t<C>> range) {
        { container.max_size() } -> std::convertible_to<std::size_t>;
        container.assign_range(range);
    };

// a container to store a protobuf string
// eg: std::string or ppb::InplaceString<N>
template <class C>
concept StringContainer = ByteContainer<C> && std::same_as<std::ranges::range_value_t<C>, char>;

// a container to store a protobuf bytes
// eg: std::vector<std::byte>, std::string or ppb::InplaceBytes<N>
template <class C>
concept BytesContainer =
    ByteContainer<C> && (std::same_as<std::ranges::range_value_t<C>, char> ||
                         std::same_as<std::ranges::range_value_t<C>, unsigned char> ||
                         std::same_as<std::ranges::range_value_t<C>, std::byte>);

}  // namespace detail

// protobuf string type: utf-8 text
struct String {
    template <class C>
    static constexpr bool stores = detail::StringContainer<C>;

    static constexpr bool validates_utf8 = true;
};

// prptobuf bytes type
struct Bytes {
    template <class C>
    static constexpr bool stores = detail::BytesContainer<C>;

    static constexpr bool validates_utf8 = false;
};

// if the type is represented on the wire starting with its length then that many bytes
template <class T>
concept LengthDelimited = std::same_as<T, String> || std::same_as<T, Bytes>;

}  // namespace ppb
