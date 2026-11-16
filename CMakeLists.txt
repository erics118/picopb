cmake_minimum_required(VERSION 3.28)

project(picopb VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(${PROJECT_NAME}-warnings INTERFACE)
target_compile_options(${PROJECT_NAME}-warnings INTERFACE
    -Wall
    -Wextra
    -Wpedantic
)

add_library(${PROJECT_NAME} INTERFACE)
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})
target_include_directories(${PROJECT_NAME} INTERFACE include)
target_compile_features(${PROJECT_NAME} INTERFACE cxx_std_23)

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

    add_executable(${PROJECT_NAME}-tests
        tests/main.cpp
        tests/picopb.cpp
        tests/libprotobuf.cpp
        ${descriptor_set}
    )
    target_link_libraries(${PROJECT_NAME}-tests PRIVATE
        ${PROJECT_NAME}::${PROJECT_NAME}
        ${PROJECT_NAME}-warnings
        doctest::doctest
        protobuf::libprotobuf
    )
    target_compile_definitions(${PROJECT_NAME}-tests PRIVATE
        PICOPB_TEST_DESCRIPTOR_SET="${descriptor_set}"
    )
    include(doctest)
    doctest_discover_tests(${PROJECT_NAME}-tests)
endif()
