#ifndef SMART_SHOVEL_FIXED_RING_BUFFER_HPP
#define SMART_SHOVEL_FIXED_RING_BUFFER_HPP

#include <array>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace smart_shovel {

enum class RingPushResult {
  inserted,
  overwritten_oldest,
};

// FixedRingBuffer has value semantics and never allocates. Copy operations
// preserve all values and logical order. Move operations transfer values and
// leave the source empty and reusable.
template <typename T, std::size_t Capacity>
class FixedRingBuffer {
  static_assert(Capacity > 0U, "FixedRingBuffer capacity must be positive");

 public:
  using value_type = T;

  constexpr FixedRingBuffer() = default;
  constexpr FixedRingBuffer(const FixedRingBuffer&) = default;
  constexpr FixedRingBuffer& operator=(const FixedRingBuffer&) = default;

  constexpr FixedRingBuffer(FixedRingBuffer&& other) noexcept(
      std::is_nothrow_move_constructible<std::array<T, Capacity>>::value)
      : data_(std::move(other.data_)), oldest_(other.oldest_), count_(other.count_) {
    other.clear();
  }

  constexpr FixedRingBuffer& operator=(FixedRingBuffer&& other) noexcept(
      std::is_nothrow_move_assignable<std::array<T, Capacity>>::value) {
    if (this != &other) {
      data_ = std::move(other.data_);
      oldest_ = other.oldest_;
      count_ = other.count_;
      other.clear();
    }
    return *this;
  }

  constexpr void clear() noexcept {
    oldest_ = 0U;
    count_ = 0U;
  }

  [[nodiscard]] constexpr bool empty() const noexcept {
    return count_ == 0U;
  }
  [[nodiscard]] constexpr bool full() const noexcept {
    return count_ == Capacity;
  }
  [[nodiscard]] constexpr std::size_t count() const noexcept {
    return count_;
  }
  [[nodiscard]] static constexpr std::size_t capacity() noexcept {
    return Capacity;
  }

  constexpr RingPushResult push_back(const T& value) {
    if (full()) {
      data_[oldest_] = value;
      oldest_ = next_index(oldest_);
      return RingPushResult::overwritten_oldest;
    }

    data_[physical_index(count_)] = value;
    ++count_;
    return RingPushResult::inserted;
  }

  constexpr RingPushResult push_back(T&& value) {
    if (full()) {
      data_[oldest_] = std::move(value);
      oldest_ = next_index(oldest_);
      return RingPushResult::overwritten_oldest;
    }

    data_[physical_index(count_)] = std::move(value);
    ++count_;
    return RingPushResult::inserted;
  }

  // Indexing is in oldest-to-newest order. operator[] is intentionally
  // unchecked for deterministic embedded use; get() is the checked form.
  constexpr T& operator[](std::size_t oldest_order_index) noexcept {
    return data_[physical_index(oldest_order_index)];
  }

  constexpr const T& operator[](std::size_t oldest_order_index) const noexcept {
    return data_[physical_index(oldest_order_index)];
  }

  [[nodiscard]] constexpr bool get(std::size_t oldest_order_index, T& output) const {
    if (oldest_order_index >= count_) {
      return false;
    }
    output = (*this)[oldest_order_index];
    return true;
  }

  [[nodiscard]] constexpr const T* oldest() const noexcept {
    return empty() ? nullptr : &data_[oldest_];
  }

  [[nodiscard]] constexpr const T* newest() const noexcept {
    return empty() ? nullptr : &data_[physical_index(count_ - 1U)];
  }

  [[nodiscard]] bool average(double& output) const noexcept {
    if (empty()) {
      return false;
    }
    double total = 0.0;
    for (std::size_t index = 0U; index < count_; ++index) {
      total += static_cast<double>((*this)[index]);
    }
    output = total / static_cast<double>(count_);
    return true;
  }

  [[nodiscard]] bool min(T& output) const {
    if (empty()) {
      return false;
    }
    output = (*this)[0U];
    for (std::size_t index = 1U; index < count_; ++index) {
      if ((*this)[index] < output) {
        output = (*this)[index];
      }
    }
    return true;
  }

  [[nodiscard]] bool max(T& output) const {
    if (empty()) {
      return false;
    }
    output = (*this)[0U];
    for (std::size_t index = 1U; index < count_; ++index) {
      if (output < (*this)[index]) {
        output = (*this)[index];
      }
    }
    return true;
  }

 private:
  [[nodiscard]] static constexpr std::size_t next_index(std::size_t index) noexcept {
    return (index + 1U) % Capacity;
  }

  [[nodiscard]] constexpr std::size_t physical_index(
      std::size_t oldest_order_index) const noexcept {
    return (oldest_ + oldest_order_index) % Capacity;
  }

  std::array<T, Capacity> data_{};
  std::size_t oldest_{0U};
  std::size_t count_{0U};
};

}  // namespace smart_shovel

#endif
