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

# compile the encoder and decoder for the rp2040 with exceptions off, at -Os and at -O3
# (the pico sdk's default), since each level can emit different calls
# fails if either object calls anything beyond libc's memory routines and gcc's arm helpers
check-no-heap:
    mkdir -p build
    for opt in Os O3; do \
        arm-none-eabi-g++ -std=c++23 -mcpu=cortex-m0plus -mthumb -$opt -fno-exceptions \
            -fno-rtti -Wall -Wextra -Werror -Iinclude -c tests/no_heap.cpp \
            -o build/no_heap-$opt.o && \
        tools/check-no-heap.sh build/no_heap-$opt.o encode_example && \
        tools/check-no-heap.sh build/no_heap-$opt.o decode_example || exit 1; \
    done

# clang-tidy on every file the build compiles and the picopb headers they include
# any warning fails it
lint: build
    run-clang-tidy -p build -quiet -warnings-as-errors '*' \
        -header-filter '{{ justfile_directory() }}/(include|tests|examples)/.*'

# the tests, the no-heap check and clang-tidy
check: test check-no-heap lint
