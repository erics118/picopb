#include "libprotobuf.hpp"

#include <doctest/doctest.h>

#include "ints.hpp"

// protobuf's encoding docs use this example: an int32 field 1 set to 150
TEST_CASE("libprotobuf_encode gives libprotobuf's bytes") {
    using picopb::test_messages::ints::Ints;

    CHECK(test_util::hex(test_util::libprotobuf_encode<Ints>("i32: 150")) == "08 96 01");
}
