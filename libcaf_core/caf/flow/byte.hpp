// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/chunk.hpp"

namespace caf::detail {

/// Turns a sequence of bytes into a sequence of chunks.
class to_chunks_step {
public:
  using input_type = std::byte;

  using output_type = chunk;

  explicit to_chunks_step(size_t num) noexcept : num_(num) {
    // nop
  }

  template <class Next, class... Steps>
  bool on_next(const std::byte& b, Next& next, Steps&... steps) {
    buf_.push_back(b);
    if (buf_.size() == num_)
      return do_emit(next, steps...);
    return true;
  }

  template <class Next, class... Steps>
  void on_complete(Next& next, Steps&... steps) {
    if (do_emit(next, steps...))
      next.on_complete(steps...);
  }

  template <class Next, class... Steps>
  void on_error(const error& what, Next& next, Steps&... steps) {
    next.on_error(what, steps...);
  }

private:
  template <class Next, class... Steps>
  bool do_emit(Next& next, Steps&... steps) {
    chunk ch;
    ch = chunk::from_buffers(buf_);
    buf_.clear();
    return next.on_next(std::move(ch), steps...);
  }

  size_t num_;
  byte_buffer buf_;
};

} // namespace caf::detail

namespace caf::flow {

/// Provides transformations for sequences of bytes.
class byte {
public:
  /// Returns a transformation step that converts a sequence of bytes into
  /// a sequence of chunks.
  static auto to_chunks(size_t num) {
    return detail::to_chunks_step{num};
  }
};

} // namespace caf::flow
