#pragma once

#include <concepts>
#include <cstdint>
#include <picopb/primitives.hpp>
#include <tuple>

namespace ppb {

// the type of a member a member pointer points to
template <class T>
struct member_type;

template <class C, class V>
struct member_type<V C::*> {
    using type = V;
};

// one entry in the field table
// stores field number, member pointer, and the primitive type
// ensure that member type is the primitive type's cpp type
template <std::uint32_t N, auto Member, Primitive S>
    requires std::same_as<typename member_type<decltype(Member)>::type, cpp_type_t<S>>
struct Field {
    static constexpr std::uint32_t number = N;
    static constexpr auto member = Member;
    using primitive = S;
};

// whether F is a valid row of M's _fields table
// F must be a ppb::Fielde and the member pointer must actually point to a member of M
template <class F, class M>
concept FieldOf = requires(M& message) {
    requires std::same_as<F, Field<F::number, F::member, typename F::primitive>>;
    message.*F::member;
};

// whether every entry of a _fields tuple is a FieldOf<M>
template <class M, class... Fs>
constexpr bool is_field_table(const std::tuple<Fs...>& /*_fields*/) {
    return (FieldOf<Fs, M> && ...);
}

}  // namespace ppb
