#pragma once

#include <cstddef>
#include <picopb/picopb.hpp>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

// stand in for what the generator will emit from tests/proto/strings.proto
// one proto message, stored two ways, to show the container is the generator's choice
namespace picopb::test_messages::strings {

// fixed-capacity storage, for firmware
struct InplaceStrings {
    static constexpr std::string_view full_name = "picopb.test_messages.strings.Strings";

    ppb::InplaceString<16> s;
    ppb::InplaceBytes<16> b;

    static constexpr auto _fields = std::tuple{
        ppb::Field<1, &InplaceStrings::s, ppb::String>{},
        ppb::Field<2, &InplaceStrings::b, ppb::Bytes>{},
    };

    bool operator==(const InplaceStrings&) const = default;
};

// standard library storage, for a host
struct StdStrings {
    static constexpr std::string_view full_name = "picopb.test_messages.strings.Strings";

    std::string s;
    std::vector<std::byte> b;

    static constexpr auto _fields = std::tuple{
        ppb::Field<1, &StdStrings::s, ppb::String>{},
        ppb::Field<2, &StdStrings::b, ppb::Bytes>{},
    };

    bool operator==(const StdStrings&) const = default;
};

}  // namespace picopb::test_messages::strings
