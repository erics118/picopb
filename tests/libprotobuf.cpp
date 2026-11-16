#include "libprotobuf.hpp"

#include <doctest/doctest.h>

// protobuf's encoding docs use this example: field 1 set to 150
TEST_CASE("libprotobuf_encode gives libprotobuf's bytes") {
    CHECK(test::hex(test::libprotobuf_encode("Example", "a: 150")) == "08 96 01");
}
