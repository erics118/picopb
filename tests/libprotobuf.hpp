#pragma once

#include <doctest/doctest.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/text_format.h>

#include <charconv>
#include <cstddef>
#include <format>
#include <fstream>
#include <ios>
#include <memory>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

// we use libprotobuf as the reference for our tests
namespace test_util {

namespace gpb = google::protobuf;

inline std::unique_ptr<gpb::DescriptorPool> load_pool() {
    auto out = std::make_unique<gpb::DescriptorPool>();

    const std::ifstream file(PICOPB_TEST_DESCRIPTOR_SET, std::ios::binary);

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

// libprotobuf's descriptor pool, built once from every file in the test descriptor set
inline gpb::DescriptorPool& pool() {
    static std::unique_ptr<gpb::DescriptorPool> p = load_pool();
    return *p;
}

// an empty libprotobuf message of type "full_name"
inline std::unique_ptr<gpb::Message> new_message(std::string_view full_name) {
    static gpb::DynamicMessageFactory factory(&pool());

    const gpb::Descriptor* const descriptor = pool().FindMessageTypeByName(std::string(full_name));

    REQUIRE(descriptor != nullptr);

    return std::unique_ptr<gpb::Message>(factory.GetPrototype(descriptor)->New());
}

// a libprotobuf message of type "full_name", holding the values in protobuf's text format
inline std::unique_ptr<gpb::Message> from_text(std::string_view full_name, std::string_view text) {
    std::unique_ptr<gpb::Message> message = new_message(full_name);

    REQUIRE(gpb::TextFormat::ParseFromString(std::string(text), message.get()));

    return message;
}

inline std::vector<std::byte> to_bytes(const gpb::Message& message) {
    std::string bytes = message.SerializeAsString();

    auto view = std::as_bytes(std::span(bytes));

    return {view.begin(), view.end()};
}

// the bytes libprotobuf encodes for picopb message type M, holding the values in text
template <class M>
std::vector<std::byte> libprotobuf_encode(std::string_view text) {
    return to_bytes(*from_text(M::full_name, text));
}

// libprotobuf parses bytes as picopb message type M and encodes the result again
// nullopt if libprotobuf rejects the bytes; fields it doesn't know are dropped, as picopb does
template <class M>
std::optional<std::vector<std::byte>> libprotobuf_reencode(std::span<const std::byte> bytes) {
    std::unique_ptr<gpb::Message> message = new_message(M::full_name);

    if (!message->ParseFromArray(bytes.data(), static_cast<int>(bytes.size()))) {
        return std::nullopt;
    }

    message->GetReflection()->MutableUnknownFields(message.get())->Clear();

    return to_bytes(*message);
}

// the bytes as hex text, since doctest can't print a vector of bytes
inline std::string hex(std::span<const std::byte> bytes) {
    std::string out;

    for (std::byte b : bytes) {
        out += std::format("{}{:02x}", out.empty() ? "" : " ", static_cast<unsigned>(b));
    }

    return out;
}

// the bytes written as hex text, such as "08 96 01"
inline std::vector<std::byte> from_hex(std::string_view text) {
    std::vector<std::byte> out;

    for (std::size_t i = 0; i + 1 < text.size(); i += 3) {
        unsigned value = 0;

        const std::string_view pair = text.substr(i, 2);

        std::from_chars(std::to_address(pair.begin()), std::to_address(pair.end()), value, 16);
        out.push_back(static_cast<std::byte>(value));
    }

    return out;
}

}  // namespace test_util
