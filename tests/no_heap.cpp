// compiled for the rp2040 by `just check-no-heap`, which fails if this needs the heap or exceptions
#include <cstddef>
#include <cstdint>
#include <picopb/picopb.hpp>
#include <span>
#include <tuple>

// not in an unnamed namespace: the functions below take these, and must keep external linkage
// so the compiler emits them for the check to read
enum class Color : std::int32_t { COLOR_UNSPECIFIED = 0, RED = 1 };

// one field of every kind, so this reaches every codec and container
struct Everything {
    std::int32_t i32 = 0;
    std::int64_t i64 = 0;
    std::uint32_t u32 = 0;
    std::uint64_t u64 = 0;
    std::int32_t s32 = 0;
    std::int64_t s64 = 0;
    std::uint32_t fx32 = 0;
    std::uint64_t fx64 = 0;
    std::int32_t sfx32 = 0;
    std::int64_t sfx64 = 0;
    bool b = false;
    Color color = Color::COLOR_UNSPECIFIED;
    ppb::InplaceString<16> s;
    ppb::InplaceBytes<16> bytes;

    static constexpr auto _fields = std::tuple{
        ppb::Field<1, &Everything::i32, ppb::Int32>{},
        ppb::Field<2, &Everything::i64, ppb::Int64>{},
        ppb::Field<3, &Everything::u32, ppb::Uint32>{},
        ppb::Field<4, &Everything::u64, ppb::Uint64>{},
        ppb::Field<5, &Everything::s32, ppb::Sint32>{},
        ppb::Field<6, &Everything::s64, ppb::Sint64>{},
        ppb::Field<7, &Everything::fx32, ppb::Fixed32>{},
        ppb::Field<8, &Everything::fx64, ppb::Fixed64>{},
        ppb::Field<9, &Everything::sfx32, ppb::Sfixed32>{},
        ppb::Field<10, &Everything::sfx64, ppb::Sfixed64>{},
        ppb::Field<11, &Everything::b, ppb::Bool>{},
        ppb::Field<12, &Everything::color, ppb::Enum<Color>>{},
        ppb::Field<13, &Everything::s, ppb::String>{},
        ppb::Field<14, &Everything::bytes, ppb::Bytes>{},
    };
};

std::size_t encode_example(const Everything& message, std::span<std::byte> out) {
    const auto size = ppb::encode(message, out);

    return size ? *size : 0;
}

bool decode_example(std::span<const std::byte> in, Everything& out) {
    auto decoded = ppb::decode<Everything>(in);

    if (!decoded) {
        return false;
    }

    out = *decoded;

    return true;
}
