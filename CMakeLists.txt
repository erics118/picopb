cmake_minimum_required(VERSION 3.28)

project(picopb VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(picopb INTERFACE)
add_library(picopb::picopb ALIAS picopb)
target_include_directories(picopb INTERFACE include)
target_compile_features(picopb INTERFACE cxx_std_23)

# if is a subproject we dont do anything else
if(NOT PROJECT_IS_TOP_LEVEL)
    return()
endif()

option(PICOPB_SANITIZE "build everything with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
if(PICOPB_SANITIZE)
    # without -fno-sanitize-recover, ubsan prints a report and the test still passes
    add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

add_library(picopb-warnings INTERFACE)
target_compile_options(picopb-warnings INTERFACE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    # -Wconversion on gcc doesn't include signedness changes, so we need to explicitly add a warning for that
    -Wsign-conversion
    -Werror
)

# also build the examples for the rp2040
add_executable(picopb-example examples/encode.cpp)
target_link_libraries(picopb-example PRIVATE
    picopb::picopb
    picopb-warnings
)

include(CTest)
if(BUILD_TESTING)
    find_package(doctest REQUIRED)
    find_package(protobuf CONFIG REQUIRED)

    # tests/libprotobuf.hpp loads every test proto into libprotobuf from this descriptor set
    file(GLOB_RECURSE test_protos CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/proto/*.proto)
    set(descriptor_set ${CMAKE_CURRENT_BINARY_DIR}/tests.desc)
    add_custom_command(
        OUTPUT ${descriptor_set}
        COMMAND protobuf::protoc --proto_path=${CMAKE_CURRENT_SOURCE_DIR}/tests/proto
                --include_imports --descriptor_set_out=${descriptor_set} ${test_protos}
        DEPENDS ${test_protos}
    )

    add_executable(picopb-tests
        tests/main.cpp
        tests/constraints.cpp
        tests/containers.cpp
        tests/floats.cpp
        tests/ints.cpp
        tests/libprotobuf.cpp
        tests/strings.cpp
        tests/writer.cpp
        ${descriptor_set}
    )
    target_link_libraries(picopb-tests PRIVATE
        picopb::picopb
        picopb-warnings
        doctest::doctest
        protobuf::libprotobuf
    )
    target_compile_definitions(picopb-tests PRIVATE
        PICOPB_TEST_DESCRIPTOR_SET="${descriptor_set}"
    )

    add_test(NAME example COMMAND picopb-example)

    include(doctest)
    doctest_discover_tests(picopb-tests)
endif()
