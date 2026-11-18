#pragma once

#include <concepts>
#include <cstddef>
#include <expected>
#include <picopb/error.hpp>
#include <picopb/reader.hpp>
#include <picopb/wire_format.hpp>
#include <picopb/writer.hpp>
#include <span>

namespace ppb {

// a generated message
// writes its fields into Writer and reads them from Reader
template <class M>
concept Message = std::default_initializable<M> &&
                  requires(const M& message, M& out, Writer& w, Reader& r, Tag tag) {
                      message.write(w);
                      { out.read_field(r, tag) } -> std::same_as<std::expected<bool, Error>>;
                  };

// encodes message into buffer and returns the number of bytes written
template <Message M>
constexpr std::expected<std::size_t, Error> encode(const M& message, std::span<std::byte> buffer) {
    Writer w{buffer};

    message.write(w);

    if (w.overflowed()) {
        return std::unexpected(Error::BufferTooSmall);
    }

    return w.size();
}

// decodes bytes into a message of type M
// a field that's missing keeps its default, and a field that appears twice keeps the last value
template <Message M>
constexpr std::expected<M, Error> decode(std::span<const std::byte> bytes) {
    M message{};
    Reader r{bytes};

    while (!r.done()) {
        auto tag = r.read_tag();

        if (!tag) {
            return std::unexpected(tag.error());
        }

        auto known = message.read_field(r, *tag);

        if (!known) {
            return std::unexpected(known.error());
        }

        if (!*known) {
            auto skipped = r.skip(tag->wire);

            if (!skipped) {
                return std::unexpected(skipped.error());
            }
        }
    }

    return message;
}

}  // namespace ppb
