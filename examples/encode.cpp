// a minimal program for the rp2040 that encodes and decodes a message with picopb
// checks.arm in flake.nix builds it with arm-none-eabi-g++, and ctest runs it on the host

#include <array>
#include <cstddef>
#include <cstdint>
#include <picopb/picopb.hpp>
#include <span>
#include <tuple>

namespace {

enum class Color : std::int32_t { COLOR_UNSPECIFIED = 0, RED = 1 };

// one field per wire type, plus an enum
struct Reading {
    std::int32_t i32 = 0;
    std::uint32_t fx32 = 0;
    std::uint64_t fx64 = 0;
    Color color = Color::COLOR_UNSPECIFIED;

    static constexpr auto _fields = std::tuple{
        ppb::Field<1, &Reading::i32, ppb::Int32>{},
        ppb::Field<7, &Reading::fx32, ppb::Fixed32>{},
        ppb::Field<8, &Reading::fx64, ppb::Fixed64>{},
        ppb::Field<12, &Reading::color, ppb::Enum<Color>>{},
    };

    bool operator==(const Reading&) const = default;
};

constexpr Reading reading{.i32 = -1, .fx32 = 1, .fx64 = 1, .color = Color::RED};

// encodes reading into 27 bytes (11 + 5 + 9 + 2) and decodes them back
constexpr bool round_trip() {
    std::array<std::byte, 32> buffer{};
    auto size = ppb::encode(reading, buffer);

    if (!size || *size != 27) {
        return false;
    }

    auto decoded = ppb::decode<Reading>(std::span(buffer).first(*size));

    return decoded && *decoded == reading;
}

// fails the build if encode or decode can't run at compile time
static_assert(round_trip());

}  // namespace

int main() { return round_trip() ? 0 : 1; }
