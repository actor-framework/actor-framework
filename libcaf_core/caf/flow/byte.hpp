// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/chunk.hpp"
#include "caf/cow_string.hpp"
#include "caf/detail/rfc3629.hpp"
#include "caf/error.hpp"
#include "caf/sec.hpp"

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

/// Turns a sequence of bytes into a sequence of chunks based on separator.
class split_at_step {
public:
  using input_type = std::byte;

  using output_type = chunk;

  explicit split_at_step(std::byte separator) noexcept : separator_(separator) {
    // nop
  }

  template <class Next, class... Steps>
  bool on_next(const std::byte& b, Next& next, Steps&... steps) {
    if (b == separator_)
      return do_emit(next, steps...);
    buf_.push_back(b);
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

  std::byte separator_;
  byte_buffer buf_;
};

/// Turns a sequence of bytes into a sequence of cow_string based on
/// separator.
class split_as_utf8_at_step {
public:
  using input_type = std::byte;

  using output_type = cow_string;

  explicit split_as_utf8_at_step(char separator) noexcept
    : separator_(separator) {
    // nop
  }

  template <class Next, class... Steps>
  bool on_next(const std::byte& b, Next& next, Steps&... steps) {
    if (static_cast<char>(b) == separator_)
      return do_emit(next, steps...);
    buf_.push_back(b);
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
    if (buf_.empty())
      return next.on_next(cow_string{}, steps...);
    if (!detail::rfc3629::valid(buf_)) {
      buf_.clear();
      next.on_error(make_error(sec::invalid_utf8), steps...);
      return false;
    }
    std::string result;
    result.reserve(buf_.size());
    std::transform(buf_.begin(), buf_.end(), std::back_inserter(result),
                   [](std::byte b) { return static_cast<char>(b); });
    auto item = cow_string{std::move(result)};
    buf_.clear();
    return next.on_next(item, steps...);
  }

  char separator_;
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

  /// Returns a transformation step that converts a sequence of bytes into
  /// a sequence of chunks by splitting the input at a given separator.
  /// @param separator The separator to split at.
  static auto split_at(std::byte separator) noexcept {
    return detail::split_at_step{separator};
  }

  /// Returns a transformation step that converts a sequence of bytes into
  /// a sequence of (UTF-8) strings by splitting the input at a given separator.
  /// @param separator The separator to split at.
  static auto split_as_utf8_at(char separator) noexcept {
    return detail::split_as_utf8_at_step{separator};
  }
};

} // namespace caf::flow
