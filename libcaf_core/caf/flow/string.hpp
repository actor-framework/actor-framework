// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/chunk.hpp"
#include "caf/cow_string.hpp"

#include <string>

namespace caf::detail {

/// Converts "\r\n" and "\r" to "\n".
class normalize_newlines_step {
public:
  using input_type = char;

  using output_type = char;

  template <class Next, class... Steps>
  bool on_next(const char& ch, Next& next, Steps&... steps) {
    switch (ch) {
      case '\r':
        prev_ = '\r';
        return next.on_next('\n', steps...);
      case '\n':
        if (prev_ == '\r') {
          prev_ = '\0';
          return true;
        }
        [[fallthrough]];
      default:
        prev_ = ch;
        return next.on_next(ch, steps...);
    }
  }

  template <class Next, class... Steps>
  void on_complete(Next& next, Steps&... steps) {
    next.on_complete(steps...);
  }

  template <class Next, class... Steps>
  void on_error(const error& what, Next& next, Steps&... steps) {
    next.on_error(what, steps...);
  }

private:
  char prev_ = '\0';
};

/// Turns a sequence of characters into a sequence of lines.
class to_lines_step {
public:
  using input_type = char;

  using output_type = cow_string;

  template <class Next, class... Steps>
  bool on_next(const char& ch, Next& next, Steps&... steps) {
    if (ch != '\n') {
      buf_.push_back(ch);
      return true;
    }
    return do_emit(next, steps...);
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
    auto line = std::string{buf_.begin(), buf_.end()};
    buf_.clear();
    return next.on_next(cow_string{std::move(line)}, steps...);
  }

  std::vector<char> buf_;
};

/// Turns a sequence of strings into a sequence of characters.
class to_chars_step {
public:
  using input_type = cow_string;

  using output_type = char;

  to_chars_step() = default;

  explicit to_chars_step(std::string_view separator) : separator_(separator) {
    // nop
  }

  template <class Next, class... Steps>
  bool on_next(const cow_string& str, Next& next, Steps&... steps) {
    for (auto ch : str) {
      if (!next.on_next(ch, steps...))
        return false;
    }
    for (auto ch : separator_) {
      if (!next.on_next(ch, steps...))
        return false;
    }
    return true;
  }

  template <class Next, class... Steps>
  void on_complete(Next& next, Steps&... steps) {
    next.on_complete(steps...);
  }

  template <class Next, class... Steps>
  void on_error(const error& what, Next& next, Steps&... steps) {
    next.on_error(what, steps...);
  }

private:
  std::string_view separator_;
};

} // namespace caf::detail

namespace caf::flow {

/// Provides transformations for strings and sequences of characters.
class string {
public:
  /// Returns a transformation step that converts "\r\n" and "\r" to "\n" in a
  /// sequence of characters.
  static detail::normalize_newlines_step normalize_newlines() {
    return {};
  }

  /// Returns a transformation step that converts a sequence of characters into
  /// a sequence of lines.
  static detail::to_lines_step to_lines() {
    return {};
  }

  /// Returns a transformation step that splits a sequence of strings into a
  /// sequence of characters.
  static detail::to_chars_step to_chars() {
    return {};
  }

  /// Returns a transformation step that splits a sequence of strings into a
  /// sequence of characters, inserting `separator` after each string.
  static detail::to_chars_step to_chars(std::string_view separator) {
    return detail::to_chars_step{separator};
  }
};

} // namespace caf::flow
