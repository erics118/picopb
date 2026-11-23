#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <picopb/error.hpp>
#include <picopb/types/field_type.hpp>
#include <picopb/utf8.hpp>
#include <picopb/wire/reader.hpp>
#include <picopb/wire/wire_type.hpp>
#include <picopb/wire/writer.hpp>
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
struct String {};

// protobuf bytes type
struct Bytes {};

namespace detail {

// an empty value isn't written, since the field has implicit presence
template <class V>
constexpr void write_len_field(Writer& w, std::uint32_t number, const V& value) {
    if (std::ranges::empty(value)) {
        return;
    }

    w.write_tag(number, WireType::Len);
    w.write_len(std::span(std::ranges::data(value), std::ranges::size(value)));
}

template <bool ValidatesUtf8, class V>
constexpr std::expected<bool, Error> read_len_field(Reader& r, WireType wire, V& value) {
    if (wire != WireType::Len) {
        return false;
    }

    const auto bytes = r.read_len();

    if (!bytes) {
        return std::unexpected(bytes.error());
    }

    if (bytes->size() > value.max_size()) {
        return std::unexpected(Error::CapacityExceeded);
    }

    if constexpr (ValidatesUtf8) {
        if (!is_valid_utf8(*bytes)) {
            return std::unexpected(Error::InvalidUtf8);
        }
    }

    using Element = std::ranges::range_value_t<V>;

    // the max_size check above means this fits; std::string's and std::vector's
    // versions return nothing
    (void)value.assign_range(*bytes | std::views::transform([](std::byte b) {
        return static_cast<Element>(b);
    }));

    return true;
}

template <>
struct FieldTypeTraits<String> {
    template <class V>
    static constexpr bool stores = StringContainer<V>;

    template <class V>
    static constexpr void write(Writer& w, std::uint32_t number, const V& value) {
        write_len_field(w, number, value);
    }

    // proto3 rejects a string that isn't valid utf-8
    template <class V>
    [[nodiscard]] static constexpr std::expected<bool, Error> read(Reader& r, WireType wire,
                                                                   V& value) {
        return read_len_field<true>(r, wire, value);
    }
};

template <>
struct FieldTypeTraits<Bytes> {
    template <class V>
    static constexpr bool stores = BytesContainer<V>;

    template <class V>
    static constexpr void write(Writer& w, std::uint32_t number, const V& value) {
        write_len_field(w, number, value);
    }

    template <class V>
    [[nodiscard]] static constexpr std::expected<bool, Error> read(Reader& r, WireType wire,
                                                                   V& value) {
        return read_len_field<false>(r, wire, value);
    }
};

}  // namespace detail

}  // namespace ppb
