#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <type_traits>
#include <utility>

namespace ppb {

// wire type determines how a field's value is encoded
// stored in the lowest 3 bits of the tag
// 3 and 4 are deprecated
enum class WireType : std::uint8_t {
    // base 128 varint, 1-10 bytes
    // lowest 7 bits are data, MSB is a continuation bit
    // used for: int32, int64, uint32, uint64, sint32, sint64, bool, enum
    Varint = 0,

    // 8 bytes
    // used for: fixed64, sfixed64, double
    I64 = 1,

    // a length (encoded as a varint), then that many bytes of data
    // used for: string, bytes, embedded messages, packed repeated fields
    Len = 2,

    // 4 bytes
    // used for: fixed32, sfixed32, float
    I32 = 5,
};

// zigzag encoding for sint32 and sint64
// n >= 0 gets turned into 2n; n < 0 becomes -2n-1
constexpr std::uint32_t zigzag32(std::int32_t value) {
    return (static_cast<std::uint32_t>(value) << 1) ^ static_cast<std::uint32_t>(value >> 31);
}

constexpr std::uint64_t zigzag64(std::int64_t value) {
    return (static_cast<std::uint64_t>(value) << 1) ^ static_cast<std::uint64_t>(value >> 63);
}

// why an encode or decode failed
enum class Error : std::uint8_t {
    // the message's encoding needs more bytes than the buffer has
    BufferTooSmall,

    // input ends in the middle of a field
    Truncated,

    // varint runs past 10 bytes
    InvalidVarint,

    // tag doesn't fit in 32 bits, has field number 0, or has wire type other than the ones defined
    InvalidTag,
};

// the field number and wire type at the start of every field
struct Tag {
    std::uint32_t field_number;
    WireType wire;
};

// writes wire format bytes into a caller's buffer
// a byte that doesn't fit is dropped and sets overflowed(), and the output is then incomplete
class Writer {
   private:
    // writes a field and value
    // if the field has implicit presence, a 0 is skipped
    template <WireType wire>
        requires(wire != WireType::Len)
    constexpr void write_field(std::uint32_t field_number, std::uint64_t bits) {
        if (bits == 0) {
            return;
        }

        write_tag(field_number, wire);

        switch (wire) {
            case WireType::Varint:
                write_varint(bits);
                break;
            case WireType::I32:
                write_i32(static_cast<std::uint32_t>(bits));
                break;
            case WireType::I64:
                write_i64(bits);
                break;
        }
    }

    constexpr void write_little_endian(std::uint64_t value, int bytes) {
        for (int i = 0; i < bytes; ++i) {
            put(static_cast<std::byte>(value & 0xff));
            value >>= 8;
        }
    }

    constexpr void put(std::byte b) {
        if (size_ == out_.size()) {
            overflowed_ = true;

            return;
        }

        out_[size_++] = b;
    }

    std::span<std::byte> out_;
    std::size_t size_ = 0;
    bool overflowed_ = false;

   public:
    constexpr explicit Writer(std::span<std::byte> out) : out_(out) {}

    // the number of bytes written so far
    [[nodiscard]] constexpr std::size_t size() const { return size_; }

    // whether a byte didn't fit and was dropped; filling the buffer exactly doesn't count
    [[nodiscard]] constexpr bool overflowed() const { return overflowed_; }

    constexpr void write_varint(std::uint64_t value) {
        // the rest doesn't fit in 7 bits, so set the continuation bit
        while (value >= 0x80) {
            put(static_cast<std::byte>((value & 0x7f) | 0x80));
            value >>= 7;
        }

        put(static_cast<std::byte>(value));
    }

    constexpr void write_i32(std::uint32_t value) { write_little_endian(value, 4); }

    constexpr void write_i64(std::uint64_t value) { write_little_endian(value, 8); }

    constexpr void write_tag(std::uint32_t field_number, WireType wire) {
        write_varint((static_cast<std::uint64_t>(field_number) << 3) |
                     static_cast<std::uint8_t>(wire));
    }

    // negative value is widened to 64 bits in two's complement
    constexpr void write_int32(std::uint32_t field_number, std::int32_t value) {
        write_field<WireType::Varint>(field_number, static_cast<std::uint64_t>(value));
    }

    constexpr void write_int64(std::uint32_t field_number, std::int64_t value) {
        write_field<WireType::Varint>(field_number, static_cast<std::uint64_t>(value));
    }

    constexpr void write_uint32(std::uint32_t field_number, std::uint32_t value) {
        write_field<WireType::Varint>(field_number, value);
    }

    constexpr void write_uint64(std::uint32_t field_number, std::uint64_t value) {
        write_field<WireType::Varint>(field_number, value);
    }

    constexpr void write_sint32(std::uint32_t field_number, std::int32_t value) {
        write_field<WireType::Varint>(field_number, zigzag32(value));
    }

    constexpr void write_sint64(std::uint32_t field_number, std::int64_t value) {
        write_field<WireType::Varint>(field_number, zigzag64(value));
    }

    constexpr void write_fixed32(std::uint32_t field_number, std::uint32_t value) {
        write_field<WireType::I32>(field_number, value);
    }

    constexpr void write_fixed64(std::uint32_t field_number, std::uint64_t value) {
        write_field<WireType::I64>(field_number, value);
    }

    // sfixed32 and sfixed64 write the value's bits unchanged, with no ZigZag or widening
    constexpr void write_sfixed32(std::uint32_t field_number, std::int32_t value) {
        write_field<WireType::I32>(field_number, static_cast<std::uint32_t>(value));
    }

    constexpr void write_sfixed64(std::uint32_t field_number, std::int64_t value) {
        write_field<WireType::I64>(field_number, static_cast<std::uint64_t>(value));
    }

    constexpr void write_bool(std::uint32_t field_number, bool value) {
        write_field<WireType::Varint>(field_number, value ? 1U : 0U);
    }

    // enums are encoded as if it were an int32 (ie as a varint)
    // values fit inside a std::int32_t, so they are generated as so
    // `requires` that E is an enum and matches the type of a std::uint32_t
    template <class E>
        requires std::is_enum_v<E> && std::is_same_v<std::underlying_type_t<E>, std::int32_t>
    constexpr void write_enum(std::uint32_t field_number, E value) {
        write_int32(field_number, std::to_underlying(value));
    }
};

// reads bytes
class Reader {
   private:
    // reads a varint, i32 or i64 value, enums included, and stores from_bits(bits) in value
    // false means the field has another wire type, so the caller skips it like an unknown field
    template <WireType expected_wire, class T, class FromBits>
        requires(expected_wire != WireType::Len)
    constexpr std::expected<bool, Error> read_field(WireType wire, T& value, FromBits from_bits)
        requires requires(std::uint64_t bits) {
            { from_bits(bits) } -> std::same_as<T>;
        }
    {
        if (wire != expected_wire) {
            return false;
        }

        std::expected<std::uint64_t, Error> bits = 0;

        switch (expected_wire) {
            case WireType::Varint:
                bits = read_varint();
                break;
            case WireType::I32:
                bits = read_little_endian(4);
                break;
            case WireType::I64:
                bits = read_little_endian(8);
                break;
        }

        if (!bits) {
            return std::unexpected(bits.error());
        }

        value = from_bits(*bits);

        return true;
    }

    constexpr std::expected<std::uint64_t, Error> read_little_endian(std::size_t bytes) {
        if (bytes > in_.size() - pos_) {
            return std::unexpected(Error::Truncated);
        }

        std::uint64_t value = 0;

        for (std::size_t i = 0; i < bytes; ++i) {
            value |= std::to_integer<std::uint64_t>(in_[pos_++]) << (8 * i);
        }

        return value;
    }

    constexpr std::expected<void, Error> advance(std::uint64_t bytes) {
        if (bytes > in_.size() - pos_) {
            return std::unexpected(Error::Truncated);
        }

        pos_ += static_cast<std::size_t>(bytes);

        return {};
    }

    std::span<const std::byte> in_;
    std::size_t pos_ = 0;

   public:
    constexpr explicit Reader(std::span<const std::byte> in) : in_(in) {}

    // whether every byte has been read
    [[nodiscard]] constexpr bool done() const { return pos_ == in_.size(); }

    constexpr std::expected<std::uint64_t, Error> read_varint() {
        std::uint64_t value = 0;

        // 10 bytes of 7 bits each cover all 64 bits
        for (int shift = 0; shift < 70; shift += 7) {
            if (pos_ == in_.size()) {
                return std::unexpected(Error::Truncated);
            }

            auto b = std::to_integer<std::uint64_t>(in_[pos_++]);

            value |= (b & 0x7f) << shift;

            // no continuation bit, so this byte is the last
            if ((b & 0x80) == 0) {
                return value;
            }
        }

        return std::unexpected(Error::InvalidVarint);
    }

    constexpr std::expected<Tag, Error> read_tag() {
        auto bits = read_varint();

        if (!bits) {
            return std::unexpected(bits.error());
        }

        auto wire = static_cast<WireType>(*bits & 7);
        std::uint64_t field_number = *bits >> 3;
        bool known_wire = wire == WireType::Varint || wire == WireType::I64 ||
                          wire == WireType::Len || wire == WireType::I32;

        if (*bits > std::numeric_limits<std::uint32_t>::max() || field_number == 0 || !known_wire) {
            return std::unexpected(Error::InvalidTag);
        }

        return Tag{
            .field_number = static_cast<std::uint32_t>(field_number),
            .wire = wire,
        };
    }

    // steps over a field's value, for a field the message doesn't know
    constexpr std::expected<void, Error> skip(WireType wire) {
        switch (wire) {
            case WireType::Varint: {
                auto value = read_varint();

                if (!value) {
                    return std::unexpected(value.error());
                }

                return {};
            }
            case WireType::I64:
                return advance(8);
            case WireType::Len: {
                auto length = read_varint();

                if (!length) {
                    return std::unexpected(length.error());
                }

                return advance(*length);
            }
            case WireType::I32:
                return advance(4);
        }

        return std::unexpected(Error::InvalidTag);
    }

    // an int32 is sign-extended to 64 bits when written, so its value is the low 32 bits
    constexpr std::expected<bool, Error> read_int32(WireType wire, std::int32_t& value) {
        return read_field<WireType::Varint>(
            wire, value, [](std::uint64_t bits) { return static_cast<std::int32_t>(bits); });
    }

    constexpr std::expected<bool, Error> read_int64(WireType wire, std::int64_t& value) {
        return read_field<WireType::Varint>(
            wire, value, [](std::uint64_t bits) { return static_cast<std::int64_t>(bits); });
    }

    constexpr std::expected<bool, Error> read_uint32(WireType wire, std::uint32_t& value) {
        return read_field<WireType::Varint>(
            wire, value, [](std::uint64_t bits) { return static_cast<std::uint32_t>(bits); });
    }

    constexpr std::expected<bool, Error> read_uint64(WireType wire, std::uint64_t& value) {
        return read_field<WireType::Varint>(wire, value, [](std::uint64_t bits) { return bits; });
    }

    // undo zigzag32
    constexpr std::expected<bool, Error> read_sint32(WireType wire, std::int32_t& value) {
        return read_field<WireType::Varint>(wire, value, [](std::uint64_t bits) {
            auto n = static_cast<std::uint32_t>(bits);

            return static_cast<std::int32_t>((n >> 1) ^ (0U - (n & 1U)));
        });
    }

    // undo zigzag64
    constexpr std::expected<bool, Error> read_sint64(WireType wire, std::int64_t& value) {
        return read_field<WireType::Varint>(wire, value, [](std::uint64_t bits) {
            return static_cast<std::int64_t>((bits >> 1) ^ (0U - (bits & 1U)));
        });
    }

    constexpr std::expected<bool, Error> read_fixed32(WireType wire, std::uint32_t& value) {
        return read_field<WireType::I32>(
            wire, value, [](std::uint64_t bits) { return static_cast<std::uint32_t>(bits); });
    }

    constexpr std::expected<bool, Error> read_fixed64(WireType wire, std::uint64_t& value) {
        return read_field<WireType::I64>(wire, value, [](std::uint64_t bits) { return bits; });
    }

    constexpr std::expected<bool, Error> read_sfixed32(WireType wire, std::int32_t& value) {
        return read_field<WireType::I32>(
            wire, value, [](std::uint64_t bits) { return static_cast<std::int32_t>(bits); });
    }

    constexpr std::expected<bool, Error> read_sfixed64(WireType wire, std::int64_t& value) {
        return read_field<WireType::I64>(
            wire, value, [](std::uint64_t bits) { return static_cast<std::int64_t>(bits); });
    }

    // any value other than 0 is true
    constexpr std::expected<bool, Error> read_bool(WireType wire, bool& value) {
        return read_field<WireType::Varint>(wire, value,
                                            [](std::uint64_t bits) { return bits != 0; });
    }

    // an enum is read like an int32, and keeps values it has no name for
    template <class E>
        requires std::is_enum_v<E> && std::is_same_v<std::underlying_type_t<E>, std::int32_t>
    constexpr std::expected<bool, Error> read_enum(WireType wire, E& value) {
        return read_field<WireType::Varint>(wire, value, [](std::uint64_t bits) {
            return static_cast<E>(static_cast<std::int32_t>(bits));
        });
    }
};

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
