#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace fw {

// Fixed-capacity ring buffer. Storage lives inside the object, so there is no
// heap use; once full, each push overwrites the oldest value.
template <typename T, std::size_t N>
class RingBuffer {
  static_assert(N > 0, "capacity must be positive");

 public:
  void push(T v) {
    data_[head_] = v;
    head_ = (head_ + 1) % N;
    if (size_ < N) ++size_;
  }

  void clear() {
    head_ = 0;
    size_ = 0;
  }

  std::size_t size() const { return size_; }
  bool full() const { return size_ == N; }
  T newest() const { return data_[(head_ + N - 1) % N]; }

  // Integer mean of the stored values. Caller makes sure size() > 0.
  std::int32_t mean() const {
    std::int64_t sum = 0;
    for (std::size_t i = 0; i < size_; ++i) sum += data_[i];
    return static_cast<std::int32_t>(sum / static_cast<std::int64_t>(size_));
  }

 private:
  std::array<T, N> data_{};
  std::size_t head_ = 0;
  std::size_t size_ = 0;
};

}  // namespace fw
