// the compile-time rules picopb's concepts and constraints enforce
// each static_assert fails the build if an edit loosens or breaks a rule

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <picopb/picopb.hpp>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace {

enum class Color : std::int32_t { RED = 1 };
enum class Small : std::uint8_t { ONE = 1 };

// the number check fails on the missing cpp_type first, so clang sees wire as never read
struct NoCppType {
    [[maybe_unused]] static constexpr ppb::detail::WireType wire = ppb::detail::WireType::Varint;

    static constexpr std::uint64_t to_bits(std::int32_t /*value*/) { return 0; }

    static constexpr std::int32_t from_bits(std::uint64_t /*bits*/) { return 0; }
};

struct LenWire {
    using cpp_type = std::int32_t;
    static constexpr ppb::detail::WireType wire = ppb::detail::WireType::Len;

    static constexpr std::uint64_t to_bits(cpp_type /*value*/) { return 0; }

    static constexpr cpp_type from_bits(std::uint64_t /*bits*/) { return 0; }
};

struct WrongFromBits {
    using cpp_type = std::int32_t;
    static constexpr ppb::detail::WireType wire = ppb::detail::WireType::Varint;

    static constexpr std::uint64_t to_bits(cpp_type /*value*/) { return 0; }

    static constexpr std::int64_t from_bits(std::uint64_t /*bits*/) { return 0; }
};

// wire type 3, a deprecated group start, isn't a value
struct Wire3 {
    using cpp_type = std::int32_t;
    static constexpr ppb::detail::WireType wire = static_cast<ppb::detail::WireType>(3);

    static constexpr std::uint64_t to_bits(cpp_type /*value*/) { return 0; }

    static constexpr cpp_type from_bits(std::uint64_t /*bits*/) { return 0; }
};

// a byte vector without assign_range, which decode copies a string or bytes value with
struct NoAssignRange : std::vector<std::byte> {
    template <class R>
    void assign_range(R&&) = delete;
};

// whether t.view() compiles, as a concept so a missing or private view() fails softly
template <class T>
concept HasView = requires(const T& t) { t.view(); };

// every built-in number type and an int32 enum are field types, and these aren't
static_assert(ppb::detail::FieldType<ppb::Int32>);
static_assert(ppb::detail::FieldType<ppb::Int64>);
static_assert(ppb::detail::FieldType<ppb::Uint32>);
static_assert(ppb::detail::FieldType<ppb::Uint64>);
static_assert(ppb::detail::FieldType<ppb::Sint32>);
static_assert(ppb::detail::FieldType<ppb::Sint64>);
static_assert(ppb::detail::FieldType<ppb::Fixed32>);
static_assert(ppb::detail::FieldType<ppb::Fixed64>);
static_assert(ppb::detail::FieldType<ppb::Sfixed32>);
static_assert(ppb::detail::FieldType<ppb::Sfixed64>);
static_assert(ppb::detail::FieldType<ppb::Bool>);
static_assert(ppb::detail::FieldType<ppb::Double>);
static_assert(ppb::detail::FieldType<ppb::Float>);
static_assert(ppb::detail::FieldType<Color>);
static_assert(!ppb::detail::FieldType<Small>);
static_assert(!ppb::detail::FieldType<std::int32_t>);
static_assert(!ppb::detail::FieldType<NoCppType>);
static_assert(!ppb::detail::FieldType<LenWire>);
static_assert(!ppb::detail::FieldType<WrongFromBits>);
static_assert(!ppb::detail::FieldType<Wire3>);

// a bytes field's container: resizable, contiguous one-byte elements
static_assert(ppb::detail::BytesContainer<std::string>);
static_assert(ppb::detail::BytesContainer<std::vector<std::byte>>);
static_assert(ppb::detail::BytesContainer<ppb::InplaceString<8>>);
static_assert(ppb::detail::BytesContainer<ppb::InplaceBytes<8>>);
static_assert(!ppb::detail::BytesContainer<std::array<char, 8>>);
static_assert(!ppb::detail::BytesContainer<std::string_view>);
static_assert(!ppb::detail::BytesContainer<std::vector<std::int32_t>>);
static_assert(!ppb::detail::BytesContainer<NoAssignRange>);

// a string field's container: the same, with char elements
static_assert(ppb::detail::StringContainer<std::string>);
static_assert(ppb::detail::StringContainer<ppb::InplaceString<8>>);
static_assert(!ppb::detail::StringContainer<ppb::InplaceBytes<8>>);
static_assert(!ppb::detail::StringContainer<std::string_view>);

// text can't be passed or bound as a vector of chars
static_assert(!std::convertible_to<ppb::InplaceString<8>&, ppb::InplaceVector<char, 8>&>);
static_assert(!std::convertible_to<ppb::InplaceString<8>, ppb::InplaceVector<char, 8>>);

// text becomes a std::string_view by conversion, as std::string does, with no view() member
static_assert(std::convertible_to<const ppb::InplaceString<8>&, std::string_view>);
static_assert(!HasView<ppb::InplaceString<8>>);

// strings and bytes are field types too
static_assert(ppb::detail::FieldType<ppb::String>);
static_assert(ppb::detail::FieldType<ppb::Bytes>);

struct M {
    std::int32_t i32 = 0;
    std::uint32_t u32 = 0;
    std::int64_t i64 = 0;
    std::string text;
    ppb::InplaceString<8> inplace_text;
    std::vector<std::byte> data;
    std::vector<unsigned char> raw;
    ppb::InplaceBytes<8> inplace_data;
    std::vector<std::int32_t> numbers;
    Color color = Color::RED;
    double d = 0;
    float f = 0;

    [[nodiscard]] std::int32_t get() const { return i32; }
};

// Field takes only a pointer to a data member that can hold its proto type
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
static_assert(FieldAccepted<&M::text, ppb::String>);
static_assert(FieldAccepted<&M::inplace_text, ppb::String>);
static_assert(FieldAccepted<&M::data, ppb::Bytes>);
static_assert(FieldAccepted<&M::text, ppb::Bytes>);
static_assert(!FieldAccepted<&M::data, ppb::String>);
static_assert(!FieldAccepted<&M::i32, ppb::String>);
static_assert(!FieldAccepted<&M::text, ppb::Int32>);

// a string needs char elements, while bytes also take unsigned char and std::byte
static_assert(FieldAccepted<&M::raw, ppb::Bytes>);
static_assert(FieldAccepted<&M::inplace_data, ppb::Bytes>);
static_assert(!FieldAccepted<&M::inplace_data, ppb::String>);
static_assert(!FieldAccepted<&M::numbers, ppb::Bytes>);

// a double field holds a double and a float field a float, never each other or an integer
static_assert(FieldAccepted<&M::d, ppb::Double>);
static_assert(FieldAccepted<&M::f, ppb::Float>);
static_assert(!FieldAccepted<&M::f, ppb::Double>);
static_assert(!FieldAccepted<&M::d, ppb::Float>);
static_assert(!FieldAccepted<&M::i32, ppb::Float>);

// an enum field holds exactly its enum
static_assert(FieldAccepted<&M::color, Color>);
static_assert(!FieldAccepted<&M::i32, Color>);
static_assert(!FieldAccepted<&M::color, ppb::Int32>);

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
