#pragma once

#include <concepts>
#include <cstdint>
#include <expected>
#include <picopb/error.hpp>
#include <picopb/types/field_type.hpp>
#include <picopb/types/numbers.hpp>
#include <picopb/types/strings.hpp>
#include <picopb/wire/reader.hpp>
#include <picopb/wire/wire_type.hpp>
#include <picopb/wire/writer.hpp>
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

}  // namespace detail

// one entry in the field table: field number, member pointer, and field type
// the member's type must be one the field type can be stored in
template <std::uint32_t N, auto Member, class T>
    requires detail::FieldType<T> && detail::FieldTypeTraits<T>::template
stores<typename detail::member_type<decltype(Member)>::type> struct Field {
    static constexpr std::uint32_t number = N;
    static constexpr auto member = Member;
    using type = T;

    // writes this field of message, unless it holds its default value
    template <class M>
    static constexpr void write(detail::Writer& w, const M& message) {
        detail::FieldTypeTraits<T>::write(w, N, message.*Member);
    }

    // reads this field of message
    // false means the field arrived with another wire type, so the caller skips it
    template <class M>
    [[nodiscard]] static constexpr std::expected<bool, Error> read(detail::Reader& r,
                                                                   detail::WireType wire,
                                                                   M& message) {
        return detail::FieldTypeTraits<T>::read(r, wire, message.*Member);
    }
};

namespace detail {

// whether F is a valid row of M's _fields table
// F must be a ppb::Field and the member pointer must actually point to a member of M
template <class F, class M>
concept FieldOf = requires(M& message) {
    requires std::same_as<F, Field<F::number, F::member, typename F::type>>;
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
