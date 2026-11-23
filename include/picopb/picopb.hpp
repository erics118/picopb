#pragma once

#include <concepts>
#include <cstddef>
#include <expected>
#include <picopb/containers/inplace_string.hpp>  // IWYU pragma: export
#include <picopb/containers/inplace_vector.hpp>  // IWYU pragma: export
#include <picopb/error.hpp>                      // IWYU pragma: export
#include <picopb/message.hpp>                    // IWYU pragma: export
#include <picopb/types/field_type.hpp>           // IWYU pragma: export
#include <picopb/types/numbers.hpp>              // IWYU pragma: export
#include <picopb/types/strings.hpp>              // IWYU pragma: export
#include <picopb/utf8.hpp>                       // IWYU pragma: export
#include <picopb/wire/reader.hpp>                // IWYU pragma: export
#include <picopb/wire/wire_type.hpp>             // IWYU pragma: export
#include <picopb/wire/writer.hpp>                // IWYU pragma: export
#include <span>
#include <tuple>

namespace ppb {

namespace detail {

// writes every field of message, in the order specified by _fields
template <Message M>
constexpr void write_fields(const M& message, Writer& w) {
    std::apply(
        [&](auto... field) {
            (decltype(field)::write(w, message), ...);
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
                    (result = decltype(field)::read(r, tag.wire, message), true)) ||
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
    detail::Writer w{buffer};

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
    detail::Reader r{bytes};

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
