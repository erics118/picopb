#pragma once

#include <doctest/doctest.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/text_format.h>

#include <cstddef>
#include <format>
#include <fstream>
#include <memory>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

// we use libprotobuf as the reference for our tests
namespace test {

namespace gpb = google::protobuf;

inline std::unique_ptr<gpb::DescriptorPool> load_pool() {
    auto out = std::make_unique<gpb::DescriptorPool>();

    std::ifstream file(PICOPB_TEST_DESCRIPTOR_SET, std::ios::binary);

    REQUIRE(file);

    std::stringstream bytes;
    bytes << file.rdbuf();
    gpb::FileDescriptorSet set;

    REQUIRE(set.ParseFromString(bytes.str()));

    for (const gpb::FileDescriptorProto& proto : set.file()) {
        REQUIRE(out->BuildFile(proto) != nullptr);
    }

    return out;
}

// libprotobuf's table of every message type
inline gpb::DescriptorPool& pool() {
    static std::unique_ptr<gpb::DescriptorPool> p = load_pool();
    return *p;
}

// a libprotobuf message of type "full_name", holding the values in protobuf's text format
inline std::unique_ptr<gpb::Message> from_text(std::string_view full_name, std::string_view text) {
    static gpb::DynamicMessageFactory factory(&pool());

    const gpb::Descriptor* descriptor = pool().FindMessageTypeByName(std::string(full_name));

    REQUIRE(descriptor != nullptr);

    std::unique_ptr<gpb::Message> message(factory.GetPrototype(descriptor)->New());

    REQUIRE(gpb::TextFormat::ParseFromString(std::string(text), message.get()));

    return message;
}

// extract the bytes that libprotobuf encode for a message
inline std::vector<std::byte> libprotobuf_encode(std::string_view full_name,
                                                 std::string_view text) {
    std::string bytes = from_text(full_name, text)->SerializeAsString();

    auto view = std::as_bytes(std::span(bytes));

    return {view.begin(), view.end()};
}

// write bytes as hex, bc doctest doesn't support pretty printing vector of bytes
inline std::string hex(std::span<const std::byte> bytes) {
    std::string out;

    for (std::byte b : bytes) {
        out += std::format("{}{:02x}", out.empty() ? "" : " ", static_cast<unsigned>(b));
    }

    return out;
}

}  // namespace test
