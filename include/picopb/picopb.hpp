#pragma once

#include <concepts>
#include <cstddef>
#include <expected>
#include <picopb/error.hpp>        // IWYU pragma: export
#include <picopb/field.hpp>        // IWYU pragma: export
#include <picopb/primitives.hpp>   // IWYU pragma: export
#include <picopb/reader.hpp>       // IWYU pragma: export
#include <picopb/wire_format.hpp>  // IWYU pragma: export
#include <picopb/writer.hpp>       // IWYU pragma: export
#include <span>
#include <tuple>

namespace ppb {

// a generated message
// ensures that _fields is actually a fields table
template <class M>
concept Message =
    std::default_initializable<M> && requires { requires detail::is_field_table<M>(M::_fields); };

namespace detail {

// writes every field of message, in the order specified by _fields
template <Message M>
constexpr void write_fields(const M& message, Writer& w) {
    std::apply(
        [&](auto... field) {
            (w.write<typename decltype(field)::primitive>(decltype(field)::number,
                                                          message.*decltype(field)::member),
             ...);
        },
        M::_fields);
}

// read one field into the member using the tag's field number
// false means there is no member with that field, ie unknown field
template <Message M>
constexpr std::expected<bool, Error> read_field(M& message, Reader& r, Tag tag) {
    std::expected<bool, Error> result = false;

    std::apply(
        [&](auto... field) {
            // || stops at the first field whose number matches, so at most one member is read
            (void)((tag.field_number == decltype(field)::number &&
                    (result = r.read<typename decltype(field)::primitive>(
                         tag.wire, message.*decltype(field)::member),
                     true)) ||
                   ...);
        },
        M::_fields);

    return result;
}

}  // namespace detail

// encode message into buffer and returns the number of bytes written
template <Message M>
[[nodiscard]] constexpr std::expected<std::size_t, Error> encode(const M& message,
                                                                 std::span<std::byte> buffer) {
    Writer w{buffer};

    detail::write_fields(message, w);

    if (w.overflowed()) {
        return std::unexpected(Error::BufferTooSmall);
    }

    return w.size();
}

// decode bytes into a message of type M
// missing field gets its default
// field that appears twice gets its last value
template <Message M>
[[nodiscard]] constexpr std::expected<M, Error> decode(std::span<const std::byte> bytes) {
    M message{};
    Reader r{bytes};

    while (!r.done()) {
        const auto tag = r.read_tag();

        if (!tag) {
            return std::unexpected(tag.error());
        }

        const auto known = detail::read_field(message, r, *tag);

        if (!known) {
            return std::unexpected(known.error());
        }

        if (!*known) {
            const auto skipped = r.skip(tag->wire);

            if (!skipped) {
                return std::unexpected(skipped.error());
            }
        }
    }

    return message;
}

}  // namespace ppb
