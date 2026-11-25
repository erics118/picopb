#pragma once

#include <algorithm>
#include <array>
#include <compare>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <ranges>
#include <span>
#include <type_traits>
#include <utility>

namespace ppb {

// a subset of std::inplace_vector
// doesn't throw, lazily deletes
template <class T, std::size_t N>
    requires std::default_initializable<T> && std::movable<T>
class InplaceVector {
   public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using iterator = T*;
    using const_iterator = const T*;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    constexpr InplaceVector() = default;

    static constexpr std::size_t max_size() noexcept { return N; }

    static constexpr std::size_t capacity() noexcept { return N; }

    // does nothing
    static constexpr void shrink_to_fit() noexcept {}

    [[nodiscard]] constexpr std::size_t size() const noexcept { return size_; }

    [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; }

    [[nodiscard]] constexpr T* data() noexcept { return elems_.data(); }

    [[nodiscard]] constexpr const T* data() const noexcept { return elems_.data(); }

    [[nodiscard]] constexpr T& operator[](std::size_t i) { return std::span(data(), size_)[i]; }

    [[nodiscard]] constexpr const T& operator[](std::size_t i) const {
        return std::span(data(), size_)[i];
    }

    // the vector isn't empty is the caller's precondition, as in std
    [[nodiscard]] constexpr T& front() { return (*this)[0]; }

    [[nodiscard]] constexpr const T& front() const { return (*this)[0]; }

    [[nodiscard]] constexpr T& back() { return (*this)[size_ - 1]; }

    [[nodiscard]] constexpr const T& back() const { return (*this)[size_ - 1]; }

    [[nodiscard]] constexpr iterator begin() noexcept { return elems_.data(); }

    [[nodiscard]] constexpr const_iterator begin() const noexcept { return elems_.data(); }

    [[nodiscard]] constexpr iterator end() noexcept {
        return std::next(begin(), static_cast<difference_type>(size_));
    }

    [[nodiscard]] constexpr const_iterator end() const noexcept {
        return std::next(begin(), static_cast<difference_type>(size_));
    }

    [[nodiscard]] constexpr const_iterator cbegin() const noexcept { return begin(); }

    [[nodiscard]] constexpr const_iterator cend() const noexcept { return end(); }

    [[nodiscard]] constexpr reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }

    [[nodiscard]] constexpr const_reverse_iterator rbegin() const noexcept {
        return const_reverse_iterator(end());
    }

    [[nodiscard]] constexpr reverse_iterator rend() noexcept { return reverse_iterator(begin()); }

    [[nodiscard]] constexpr const_reverse_iterator rend() const noexcept {
        return const_reverse_iterator(begin());
    }

    [[nodiscard]] constexpr const_reverse_iterator crbegin() const noexcept { return rbegin(); }

    [[nodiscard]] constexpr const_reverse_iterator crend() const noexcept { return rend(); }

    // value initialize new elements
    // returns false if size > N
    [[nodiscard]] constexpr bool resize(std::size_t size) {
        if (size <= size_) {
            size_ = size;
            return true;
        }

        return size <= N && resize(size, T{});
    }

    // new elements are copies of value
    // returns false if size > N
    [[nodiscard]] constexpr bool resize(std::size_t size, const T& value) {
        if (size > N) {
            return false;
        }

        if (size > size_) {
            auto new_elems = std::span(elems_).subspan(size_, size - size_);
            std::ranges::fill(new_elems, value);
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

    // the new element, or nullptr with nothing changed when the vector is full
    template <class... Args>
        requires std::constructible_from<T, Args...>
    [[nodiscard]] constexpr T* try_emplace_back(Args&&... args) {
        if (size_ == N) {
            return nullptr;
        }

        T& added = std::span(elems_)[size_];

        added = T(std::forward<Args>(args)...);
        ++size_;

        return &added;
    }

    [[nodiscard]] constexpr T* try_push_back(const T& value) { return try_emplace_back(value); }

    [[nodiscard]] constexpr T* try_push_back(T&& value) {
        return try_emplace_back(std::move(value));
    }

    // the vector isn't empty is the caller's precondition, as in std
    constexpr void pop_back() { --size_; }

    constexpr void clear() noexcept { size_ = 0; }

    // a literal 0 would otherwise pick erase(const_iterator) as a null pointer
    void erase(std::size_t) = delete;

    // the element after the last one removed
    constexpr iterator erase(const_iterator pos) { return erase(pos, std::next(pos)); }

    constexpr iterator erase(const_iterator first, const_iterator last) {
        T* const from = std::next(begin(), first - cbegin());
        T* const to = std::next(begin(), last - cbegin());

        std::ranges::move(to, end(), from);
        size_ -= static_cast<std::size_t>(to - from);

        return from;
    }

    // swaps all N slots, spare ones included
    constexpr void swap(InplaceVector& other) noexcept(
        std::is_nothrow_swappable_v<std::array<T, N>>) {
        std::ranges::swap(elems_, other.elems_);
        std::ranges::swap(size_, other.size_);
    }

    friend constexpr void swap(InplaceVector& a, InplaceVector& b) noexcept(noexcept(a.swap(b))) {
        a.swap(b);
    }

    friend constexpr bool operator==(const InplaceVector& a, const InplaceVector& b) {
        return std::ranges::equal(a, b);
    }

    friend constexpr auto operator<=>(const InplaceVector& a, const InplaceVector& b)
        requires std::three_way_comparable<T>
    {
        return std::lexicographical_compare_three_way(a.begin(), a.end(), b.begin(), b.end());
    }

   private:
    std::array<T, N> elems_{};
    std::size_t size_ = 0;
};

// removes every element pred is true for, and returns how many
template <class T, std::size_t N, class Pred>
constexpr std::size_t erase_if(InplaceVector<T, N>& vector, Pred pred) {
    const auto removed = std::ranges::remove_if(vector, pred);
    const auto count = removed.size();

    vector.erase(removed.begin(), removed.end());

    return count;
}

// removes every element equal to value, and returns how many
template <class T, std::size_t N, class U = T>
constexpr std::size_t erase(InplaceVector<T, N>& vector, const U& value) {
    return erase_if(vector, [&](const T& element) {
        return element == value;
    });
}

// raw bytes with capacity N, for bytes fields
template <std::size_t N>
using InplaceBytes = InplaceVector<std::byte, N>;

}  // namespace ppb
