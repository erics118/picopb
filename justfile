default: (build "Debug")

configure build_type="Debug":
    cmake -S . -B build -DCMAKE_BUILD_TYPE={{ build_type }}

build build_type="Debug": (configure build_type)
    cmake --build build

clean:
    rm -rf build build-sanitize

test build_type="Debug": (build build_type)
    ctest --test-dir build --output-on-failure

# the tests with asan and ubsan, in their own build folders
sanitize:
    cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug -DPICOPB_SANITIZE=ON
    cmake --build build-sanitize
    ctest --test-dir build-sanitize --output-on-failure
