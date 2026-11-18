// a minimal program for the rp2040 that encodes a few fields with picopb
// checks.arm in flake.nix builds it with arm-none-eabi-g++, and ctest runs it on the host

#include <array>
#include <cstddef>
#include <cstdint>
#include <picopb/picopb.hpp>
#include <span>

namespace {

enum class Color : std::int32_t { COLOR_UNSPECIFIED = 0, RED = 1 };

// one call per wire type, plus write_enum
constexpr std::size_t encode(std::span<std::byte> buffer) {
    ppb::Writer w{buffer};

    w.write_int32(1, -1);
    w.write_fixed32(7, 1);
    w.write_fixed64(8, 1);
    w.write_enum(12, Color::RED);

    return w.overflowed() ? 0 : w.size();
}

constexpr std::size_t encoded_size() {
    std::array<std::byte, 32> buffer{};

    return encode(buffer);
}

// fails the build if any writer can't run at compile time: 11 + 5 + 9 + 2 bytes
static_assert(encoded_size() == 27);

}  // namespace

int main() {
    std::array<std::byte, 64> buffer{};

    return encode(buffer) == 27 ? 0 : 1;
}
