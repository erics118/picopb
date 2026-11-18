#pragma once

#include <cstdint>
#include <limits>

// the smallest and largest value of each integer type, for test values
namespace test_util {

inline constexpr std::int32_t int32_min = std::numeric_limits<std::int32_t>::min();
inline constexpr std::int32_t int32_max = std::numeric_limits<std::int32_t>::max();
inline constexpr std::int64_t int64_min = std::numeric_limits<std::int64_t>::min();
inline constexpr std::int64_t int64_max = std::numeric_limits<std::int64_t>::max();
inline constexpr std::uint32_t uint32_max = std::numeric_limits<std::uint32_t>::max();
inline constexpr std::uint64_t uint64_max = std::numeric_limits<std::uint64_t>::max();

}  // namespace test_util
