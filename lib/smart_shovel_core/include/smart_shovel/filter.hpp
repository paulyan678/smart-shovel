#ifndef SMART_SHOVEL_FILTER_HPP
#define SMART_SHOVEL_FILTER_HPP

#include <cstddef>

#include "smart_shovel/fixed_ring_buffer.hpp"

namespace smart_shovel {

template <std::size_t WindowSize>
class MovingAverageFilter {
 public:
  [[nodiscard]] double add(double sample) noexcept {
    (void)samples_.push_back(sample);
    double result = 0.0;
    (void)samples_.average(result);
    return result;
  }

  void reset() noexcept {
    samples_.clear();
  }

  [[nodiscard]] bool ready() const noexcept {
    return samples_.full();
  }
  [[nodiscard]] std::size_t sample_count() const noexcept {
    return samples_.count();
  }
  [[nodiscard]] bool value(double& output) const noexcept {
    return samples_.average(output);
  }

 private:
  FixedRingBuffer<double, WindowSize> samples_{};
};

}  // namespace smart_shovel

#endif
