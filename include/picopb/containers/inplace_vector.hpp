#pragma once

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <ranges>
#include <span>
#include <utility>

namespace ppb {

// inplace vector implementation, subset of std::inplace_vector
template <class T, std::size_t N>
class InplaceVector {
   public:
    using value_type = T;

    constexpr InplaceVector() = default;

    static constexpr std::size_t max_size() { return N; }

    [[nodiscard]] constexpr std::size_t size() const { return size_; }

    [[nodiscard]] constexpr bool empty() const { return size_ == 0; }

    [[nodiscard]] constexpr T* data() { return elems_.data(); }

    [[nodiscard]] constexpr const T* data() const { return elems_.data(); }

    // begin and end come from one span over the elements in use, so they share its bounds,
    // and a hardened build's span iterators trap on a read past the size
    [[nodiscard]] constexpr auto begin() { return std::span(elems_.data(), size_).begin(); }

    [[nodiscard]] constexpr auto begin() const { return std::span(elems_.data(), size_).begin(); }

    [[nodiscard]] constexpr auto end() { return std::span(elems_.data(), size_).end(); }

    [[nodiscard]] constexpr auto end() const { return std::span(elems_.data(), size_).end(); }

    // new elements are value initialized
    // if size > N this is an error and returns false
    [[nodiscard]] constexpr bool resize(std::size_t size) {
        if (size > N) {
            return false;
        }

        if (size > size_) {
            std::ranges::fill(std::span(elems_).subspan(size_, size - size_), T{});
        }

        size_ = size;
        return true;
    }

    // replaces the elements with the range's, without value-initializing them first
    // if the range has more than N elements this is an error and returns false, changing nothing
    template <std::ranges::sized_range R>
        requires std::convertible_to<std::ranges::range_reference_t<R>, T>
    [[nodiscard]] constexpr bool assign_range(R&& range) {
        const auto count = static_cast<std::size_t>(std::ranges::size(range));

        if (count > N) {
            return false;
        }

        std::ranges::copy(std::forward<R>(range), elems_.begin());
        size_ = count;
        return true;
    }

    friend constexpr bool operator==(const InplaceVector& a, const InplaceVector& b) {
        return std::ranges::equal(a, b);
    }

   private:
    std::array<T, N> elems_{};
    std::size_t size_ = 0;
};

// raw bytes with capacity N, for bytes fields
template <std::size_t N>
using InplaceBytes = InplaceVector<std::byte, N>;

}  // namespace ppb
