#pragma once

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <picopb/error.hpp>
#include <picopb/types/numbers.hpp>
#include <picopb/types/strings.hpp>
#include <picopb/utf8.hpp>
#include <picopb/wire/reader.hpp>
#include <picopb/wire/wire_type.hpp>
#include <picopb/wire/writer.hpp>
#include <ranges>
#include <span>
#include <tuple>

namespace ppb {

namespace detail {

// the type of a member a member pointer points to
template <class T>
struct member_type;

template <class C, class V>
struct member_type<V C::*> {
    using type = V;
};

// whether a member of type V can hold a field of proto type T
// a primitive needs exactly its C++ type, a string or bytes field a container it stores in
template <class T, class V>
concept HoldsField = (Primitive<T> && std::same_as<V, cpp_type_t<T>>) ||
                     (LengthDelimited<T> && T::template stores<V>);

}  // namespace detail

// one entry in the field table
// stores field number, member pointer, and the proto type
// ensure that the member's type can hold the proto type
template <std::uint32_t N, auto Member, class T>
    requires detail::HoldsField<T, typename detail::member_type<decltype(Member)>::type>
struct Field {
    static constexpr std::uint32_t number = N;
    static constexpr auto member = Member;
    using proto_type = T;

    // writes this field of message
    // a 0 or an empty string isn't written, since the field has implicit presence
    template <class M>
    static constexpr void write(Writer& w, const M& message) {
        const auto& value = message.*Member;

        if constexpr (Primitive<T>) {
            if (T::to_bits(value) != 0) {
                w.write<T>(N, value);
            }
        } else {
            if (!std::ranges::empty(value)) {
                w.write_bytes(N, std::span(std::ranges::data(value), std::ranges::size(value)));
            }
        }
    }

    // reads this field of message
    // false means the field arrived with another wire type, so caller skips it
    template <class M>
    [[nodiscard]] static constexpr std::expected<bool, Error> read(Reader& r, WireType wire,
                                                                   M& message) {
        auto& value = message.*Member;

        if constexpr (Primitive<T>) {
            return r.read<T>(wire, value);
        } else {
            std::span<const std::byte> bytes;
            const auto found = r.read_bytes(wire, bytes);

            if (!found || !*found) {
                return found;
            }

            if (bytes.size() > value.max_size()) {
                return std::unexpected(Error::CapacityExceeded);
            }

            if constexpr (T::validates_utf8) {
                if (!is_valid_utf8(bytes)) {
                    return std::unexpected(Error::InvalidUtf8);
                }
            }

            using Element = std::ranges::range_value_t<decltype(value)>;

            auto copy = [&](Element* data, std::size_t size) {
                std::ranges::transform(bytes, data, [](std::byte b) {
                    return static_cast<Element>(b);
                });

                return size;
            };

            // the max_size check above means this fits; std::string's and std::vector's
            // versions return nothing
            // resize_and_overwrite skips the value initialization that resize does before the copy
            if constexpr (requires { value.resize_and_overwrite(bytes.size(), copy); }) {
                (void)value.resize_and_overwrite(bytes.size(), copy);
            } else {
                (void)value.resize(bytes.size());
                (void)copy(std::ranges::data(value), bytes.size());
            }

            return true;
        }
    }
};

namespace detail {

// whether F is a valid row of M's _fields table
// F must be a ppb::Field and the member pointer must actually point to a member of M
template <class F, class M>
concept FieldOf = requires(M& message) {
    requires std::same_as<F, Field<F::number, F::member, typename F::proto_type>>;
    message.*F::member;
};

// whether every entry of a _fields tuple is a FieldOf<M>
template <class M, class... Fs>
constexpr bool is_field_table(const std::tuple<Fs...>& /*_fields*/) {
    return (FieldOf<Fs, M> && ...);
}

}  // namespace detail

// a generated message
// ensures that _fields is actually a fields table
template <class M>
concept Message =
    std::default_initializable<M> && requires { requires detail::is_field_table<M>(M::_fields); };

}  // namespace ppb
