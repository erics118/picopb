#pragma once

#include <picopb/picopb.hpp>
#include <string_view>
#include <tuple>

// stands in for what the generator will emit from tests/proto/floats.proto
namespace picopb::test_messages::floats {

struct Floats {
    static constexpr std::string_view full_name = "picopb.test_messages.floats.Floats";

    double d = 0;
    float f = 0;

    static constexpr auto _fields = std::tuple{
        ppb::Field<1, &Floats::d, ppb::Double>{},
        ppb::Field<2, &Floats::f, ppb::Float>{},
    };
};

}  // namespace picopb::test_messages::floats
