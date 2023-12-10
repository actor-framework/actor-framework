// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/chunk.hpp"

#include <cstddef>

namespace caf::detail {

/// Turns a sequence of bytes into a sequence of chunks.
class to_chunks_step {
public:
  using input_type = std::byte;

  using output_type = chunk;

  explicit to_chunks_step(size_t chunk_size) noexcept
    : chunk_size_(chunk_size) {
    // nop
  }

  template <class Next, class... Steps>
  bool on_next(const std::byte& b, Next& next, Steps&... steps) {
    buf_.push_back(b);
    if (buf_.size() == chunk_size_)
      return do_emit(next, steps...);
    return true;
  }

  template <class Next, class... Steps>
  void on_complete(Next& next, Steps&... steps) {
    if (!buf_.empty() && !do_emit(next, steps...))
      return;
    next.on_complete(steps...);
  }

  template <class Next, class... Steps>
  void on_error(const error& what, Next& next, Steps&... steps) {
    if (!buf_.empty() && !do_emit(next, steps...))
      return;
    next.on_error(what, steps...);
  }

private:
  template <class Next, class... Steps>
  bool do_emit(Next& next, Steps&... steps) {
    auto item = chunk{buf_};
    buf_.clear();
    return next.on_next(item, steps...);
  }

  size_t chunk_size_;
  byte_buffer buf_;
};

} // namespace caf::detail

namespace caf::flow {

/// Provides transformations for sequences of bytes.
class byte {
public:
  /// Returns a transformation step that converts a sequence of bytes into
  /// a sequence of chunks.
  /// @param chunk_size The maximum number of bytes per chunk.
  static auto to_chunks(size_t chunk_size) noexcept {
    return detail::to_chunks_step{chunk_size};
  }
};

} // namespace caf::flow
