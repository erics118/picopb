#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace ppb::detail {

// a byte of text as a number; text is char when encoding and std::byte when decoding
template <class Byte>
constexpr std::uint32_t byte_value(Byte b) {
    return static_cast<unsigned char>(b);
}

// if the 8 bytes at std::text[i] is ascii
// because most text is just ascii, so this makes the check faster
template <class Byte>
constexpr bool ascii8(std::span<const Byte> text, std::size_t i) {
    if (text.size() - i < 8) {
        return false;
    }

    std::uint32_t high_bits = 0;

    for (std::size_t k = 0; k < 8; ++k) {
        high_bits |= byte_value(text[i + k]);
    }

    return (high_bits & 0x80) == 0;
}

// whether it is a continuation byte
constexpr bool is_continuation(std::uint32_t byte) { return (byte & 0xc0) == 0x80; }

// the size of the longest prefix of text, at most max bytes, that doesn't split a character
// it only avoids a split, so invalid utf-8 before the cut is kept as is
constexpr std::size_t utf8_prefix(std::string_view text, std::size_t max) {
    std::size_t size = std::min(text.size(), max);

    // text[size] is the first byte cut off: back up while it continues a character
    while (size < text.size() && size > 0 &&
           is_continuation(static_cast<unsigned char>(text[size]))) {
        --size;
    }

    return size;
}

// if is valid utf-8, which protobuf requires for string fields
// rejects overlong forms, utf-16 surrogates, and code points above U+10FFFF
template <class Byte>
    requires(sizeof(Byte) == 1)
constexpr bool is_valid_utf8(std::span<const Byte> text) {
    std::size_t i = 0;

    while (i < text.size()) {
        if (ascii8(text, i)) {
            i += 8;
            continue;
        }

        const auto lead = byte_value(text[i]);

        // ascii is one byte
        if (lead < 0x80) {
            ++i;
            continue;
        }

        // lead byte says how many continuation bytes there will be
        std::size_t extra = 0;
        std::uint32_t smallest = 0;  // the smallest code point that needs this many bytes
        std::uint32_t code = 0;

        if ((lead & 0xe0) == 0xc0) {
            extra = 1;
            smallest = 0x80;
            code = lead & 0x1f;
        } else if ((lead & 0xf0) == 0xe0) {
            extra = 2;
            smallest = 0x800;
            code = lead & 0x0f;
        } else if ((lead & 0xf8) == 0xf0) {
            extra = 3;
            smallest = 0x10000;
            code = lead & 0x07;
        } else {
            // a continuation byte with no lead, or a byte that can't start a character
            return false;
        }

        // the text ends before the character does
        if (extra >= text.size() - i) {
            return false;
        }

        for (std::size_t k = 1; k <= extra; ++k) {
            const auto next = byte_value(text[i + k]);

            // every byte after the lead must be 10xxxxxx
            if (!is_continuation(next)) {
                return false;
            }

            code = (code << 6) | (next & 0x3f);
        }

        // overlong, past the end of unicode, or a utf-16 surrogate
        if (code < smallest || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) {
            return false;
        }

        i += extra + 1;
    }

    return true;
}

}  // namespace ppb::detail
