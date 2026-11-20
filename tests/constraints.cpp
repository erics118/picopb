// the compile-time rules picopb's concepts and constraints enforce
// each static_assert fails the build if an edit loosens or breaks a rule

#include <cstdint>
#include <picopb/picopb.hpp>
#include <tuple>

namespace {

enum class Color : std::int32_t { RED = 1 };
enum class Small : std::uint8_t { ONE = 1 };

// Primitive fails on the missing cpp_type first, so clang sees wire as never read
struct NoCppType {
    [[maybe_unused]] static constexpr ppb::WireType wire = ppb::WireType::Varint;

    static constexpr std::uint64_t to_bits(std::int32_t /*value*/) { return 0; }

    static constexpr std::int32_t from_bits(std::uint64_t /*bits*/) { return 0; }
};

struct LenWire {
    using cpp_type = std::int32_t;
    static constexpr ppb::WireType wire = ppb::WireType::Len;

    static constexpr std::uint64_t to_bits(cpp_type /*value*/) { return 0; }

    static constexpr cpp_type from_bits(std::uint64_t /*bits*/) { return 0; }
};

struct WrongFromBits {
    using cpp_type = std::int32_t;
    static constexpr ppb::WireType wire = ppb::WireType::Varint;

    static constexpr std::uint64_t to_bits(cpp_type /*value*/) { return 0; }

    static constexpr std::int64_t from_bits(std::uint64_t /*bits*/) { return 0; }
};

// wire type 3, a deprecated group start, isn't a value
struct Wire3 {
    using cpp_type = std::int32_t;
    static constexpr ppb::WireType wire = static_cast<ppb::WireType>(3);

    static constexpr std::uint64_t to_bits(cpp_type /*value*/) { return 0; }

    static constexpr cpp_type from_bits(std::uint64_t /*bits*/) { return 0; }
};

// every built-in trait is a Primitive, and these aren't
static_assert(ppb::Primitive<ppb::Int32>);
static_assert(ppb::Primitive<ppb::Int64>);
static_assert(ppb::Primitive<ppb::Uint32>);
static_assert(ppb::Primitive<ppb::Uint64>);
static_assert(ppb::Primitive<ppb::Sint32>);
static_assert(ppb::Primitive<ppb::Sint64>);
static_assert(ppb::Primitive<ppb::Fixed32>);
static_assert(ppb::Primitive<ppb::Fixed64>);
static_assert(ppb::Primitive<ppb::Sfixed32>);
static_assert(ppb::Primitive<ppb::Sfixed64>);
static_assert(ppb::Primitive<ppb::Bool>);
static_assert(ppb::Primitive<ppb::Enum<Color>>);
static_assert(!ppb::Primitive<std::int32_t>);
static_assert(!ppb::Primitive<NoCppType>);
static_assert(!ppb::Primitive<LenWire>);
static_assert(!ppb::Primitive<WrongFromBits>);
static_assert(!ppb::Primitive<Wire3>);

// Enum takes only an enum whose underlying type is std::int32_t
template <class E>
concept EnumAccepted = requires { typename ppb::Enum<E>; };

static_assert(EnumAccepted<Color>);
static_assert(!EnumAccepted<Small>);
static_assert(!EnumAccepted<std::int32_t>);

struct M {
    std::int32_t i32 = 0;
    std::uint32_t u32 = 0;
    std::int64_t i64 = 0;

    [[nodiscard]] std::int32_t get() const { return i32; }
};

// Field takes only a pointer to a data member whose type is the primitive type's C++ type
template <auto Member, class S>
concept FieldAccepted = requires { typename ppb::Field<1, Member, S>; };

static_assert(FieldAccepted<&M::i32, ppb::Int32>);
static_assert(FieldAccepted<&M::i32, ppb::Sint32>);
static_assert(FieldAccepted<&M::u32, ppb::Uint32>);
static_assert(!FieldAccepted<&M::u32, ppb::Int32>);
static_assert(!FieldAccepted<&M::i64, ppb::Int32>);
static_assert(!FieldAccepted<&M::i32, ppb::Bool>);
static_assert(!FieldAccepted<std::int32_t{42}, ppb::Int32>);
static_assert(!FieldAccepted<nullptr, ppb::Int32>);
static_assert(!FieldAccepted<&M::get, ppb::Int32>);

struct Good {
    std::int32_t i32 = 0;
    static constexpr auto _fields = std::tuple{ppb::Field<1, &Good::i32, ppb::Int32>{}};
};

struct NoFields {
    std::int32_t i32 = 0;
};

struct NotTuple {
    static constexpr int _fields = 0;
};

struct NotField {
    static constexpr auto _fields = std::tuple{1};
};

// a Field whose member belongs to another struct, with a matching type
struct Borrows {
    std::int32_t i32 = 0;
    static constexpr auto _fields = std::tuple{ppb::Field<1, &M::i32, ppb::Int32>{}};
};

// looks like a Field, but pairs a std::uint32_t member with ppb::Int32, which Field rejects
struct Lying {
    std::uint32_t u32 = 0;

    struct Fake {
        static constexpr std::uint32_t number = 1;
        static constexpr auto member = &Lying::u32;
        using primitive = ppb::Int32;
    };

    static constexpr auto _fields = std::tuple{Fake{}};
};

// a Message's _fields is a tuple of Fields whose members belong to it
static_assert(ppb::Message<Good>);
static_assert(!ppb::Message<NoFields>);
static_assert(!ppb::Message<NotTuple>);
static_assert(!ppb::Message<NotField>);
static_assert(!ppb::Message<Borrows>);
static_assert(!ppb::Message<Lying>);

}  // namespace
