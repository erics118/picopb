#pragma once

#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <picopb/picopb.hpp>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "libprotobuf.hpp"

// compares picopb's encode and decode with libprotobuf's
namespace test_util {

// the bytes ppb::encode writes for message, as hex text
template <ppb::Message M>
std::string picopb_hex(const M& message) {
    std::array<std::byte, 256> buffer{};
    const auto size = ppb::encode(message, buffer);

    REQUIRE(size.has_value());

    return hex(std::span(buffer).first(*size));
}

// libprotobuf accepts the bytes only if valid, picopb agrees, and both decode the same message
template <ppb::Message M>
void check_decode(std::string_view hex_bytes, bool valid) {
    INFO(hex_bytes);

    const std::vector<std::byte> bytes = from_hex(hex_bytes);
    const auto reference = libprotobuf_reencode<M>(bytes);
    const auto decoded = ppb::decode<M>(bytes);

    REQUIRE(reference.has_value() == valid);
    REQUIRE(decoded.has_value() == valid);

    if (decoded && reference) {
        CHECK(picopb_hex(*decoded) == hex(*reference));
    }
}

}  // namespace test_util
