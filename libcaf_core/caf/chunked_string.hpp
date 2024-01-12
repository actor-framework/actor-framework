// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/detail/forward_list.hpp"
#include "caf/detail/print.hpp"
#include "caf/fwd.hpp"

#include <algorithm>
#include <cstddef>
#include <iosfwd>
#include <iterator>
#include <string>
#include <string_view>

namespace caf {

/// Represents a chunked string as a linked list of string views.
class CAF_CORE_EXPORT chunked_string {
public:
  using node_type = detail::forward_list_node<std::string_view>;

  using const_iterator = detail::forward_list_iterator<const std::string_view>;

  explicit chunked_string(const node_type* head) noexcept : head_(head) {
    // nop
  }

  chunked_string() noexcept = default;

  /// Returns an iterator to the first chunk.
  auto begin() const noexcept {
    return const_iterator{head_};
  }

  /// Returns the past-the-end iterator.
  auto end() const noexcept {
    return const_iterator{};
  }

  /// Returns the size of the string, i.e., the sum of all chunk sizes.
  size_t size() const noexcept;

  /// Copies the chunked string to an output iterator.
  template <class OutputIterator>
  OutputIterator copy_to(OutputIterator out) const {
    for (auto chunk : *this)
      out = std::copy(chunk.begin(), chunk.end(), out);
    return out;
  }

  /// Copies the quoted representation of the chunked string to an output
  /// iterator.
  template <class OutputIterator>
  OutputIterator copy_quoted_to(OutputIterator out) const {
    *out++ = '"';
    for (auto chunk : *this)
      for (auto ch : chunk)
        out = detail::print_escaped_to(out, ch);
    *out++ = '"';
    return out;
  }

private:
  const node_type* head_;
};

/// Converts a linked string chunk to a `std::string`.
CAF_CORE_EXPORT std::string to_string(const chunked_string& str);

/// Prints a chunked string to an output stream.
CAF_CORE_EXPORT std::ostream& operator<<(std::ostream& out,
                                         const chunked_string& str);

/// Builds a chunked string by allocating each chunk on a monotonic buffer.
class CAF_CORE_EXPORT chunked_string_builder {
public:
  using list_type = detail::forward_list<std::string_view>;

  using resource_type = detail::monotonic_buffer_resource;

  /// The size of a single chunk.
  static constexpr size_t chunk_size = 128;

  explicit chunked_string_builder(resource_type* resource) noexcept;

  ~chunked_string_builder() noexcept {
    // nop
  }

  /// Appends a character to the current chunk or creates a new chunk if the
  /// current chunk reached its capacity.
  void append(char ch);

  /// Seals the current chunk and returns the first chunk.
  chunked_string build();

private:
  [[nodiscard]] resource_type* resource() noexcept {
    return chunks_.get_allocator().resource();
  }

  char* current_block_ = nullptr;
  size_t write_pos_ = 0;
  union { // Suppress the dtor call for chunks_.
    list_type chunks_;
  };
};

/// An output iterator that appends characters to a linked string chunk builder.
class chunked_string_builder_output_iterator {
public:
  using difference_type = ptrdiff_t;
  using value_type = char;
  using pointer = char*;
  using reference = char&;
  using iterator_category = std::output_iterator_tag;

  explicit chunked_string_builder_output_iterator(
    chunked_string_builder* builder) noexcept
    : buiilder_(builder) {
    // nop
  }

  chunked_string_builder_output_iterator& operator=(char ch) {
    buiilder_->append(ch);
    return *this;
  }

  chunked_string_builder_output_iterator& operator*() {
    return *this;
  }

  chunked_string_builder_output_iterator& operator++() {
    return *this;
  }

  chunked_string_builder_output_iterator operator++(int) {
    return *this;
  }

private:
  chunked_string_builder* buiilder_;
};

} // namespace caf

#ifdef CAF_USE_STD_FORMAT

#  include "caf/raise_error.hpp"

#  include <format>

namespace std {

template <>
struct std::formatter<caf::chunked_string, char> {
  bool quoted = false;

  template <class ParseContext>
  constexpr ParseContext::iterator parse(ParseContext& ctx) {
    auto it = ctx.begin();
    if (it == ctx.end())
      return it;
    if (*it == '?') {
      quoted = true;
      ++it;
    }
    if (*it != '}')
      CAF_RAISE_ERROR(std::format_error,
                      "invalid format string for caf::linked_string_chunk");
    return it;
  }

  template <class FmtContext>
  auto format(const caf::chunked_string& str, FmtContext& ctx) const {
    auto out = ctx.out();
    return quoted ? str.copy_quoted_to(out) : str.copy_to(out);
  }
};

} // namespace std

#endif
