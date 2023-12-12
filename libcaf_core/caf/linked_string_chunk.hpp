// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/build_config.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/print.hpp"
#include "caf/fwd.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <string>
#include <string_view>

namespace caf {

/// Represents a chunked string that as a linked list of string views.
struct linked_string_chunk {
  /// The characters of this chunk.
  std::string_view content;

  /// Points to the next chunk in the list.
  linked_string_chunk* next = nullptr;

  /// Copies the chunked string to an output iterator.
  template <class OutputIterator>
  OutputIterator copy_to(OutputIterator out) const {
    for (const auto* chunk = this; chunk != nullptr; chunk = chunk->next)
      out = std::copy(chunk->content.begin(), chunk->content.end(), out);
    return out;
  }

  /// Copies the quoted representation of the chunked string to an output
  /// iterator.
  template <class OutputIterator>
  OutputIterator copy_quoted_to(OutputIterator out) const {
    *out++ = '"';
    for (const auto* chunk = this; chunk != nullptr; chunk = chunk->next)
      for (auto ch : chunk->content)
        out = detail::print_escaped_to(out, ch);
    *out++ = '"';
    return out;
  }
};

/// Converts a linked string chunk to a `std::string`.
CAF_CORE_EXPORT std::string to_string(const linked_string_chunk& head);

/// Builds a chunked string by allocating each chunk on a monotonic buffer.
class CAF_CORE_EXPORT linked_string_chunk_builder {
public:
  /// The size of a single chunk.
  static constexpr size_t chunk_size = 128;

  explicit linked_string_chunk_builder(
    detail::monotonic_buffer_resource* resource);

  /// Appends a character to the current chunk or creates a new chunk if the
  /// current chunk reached its capacity.
  void append(char ch);

  /// Seals the current chunk and returns the first chunk.
  linked_string_chunk* seal();

private:
  char* current_block_ = nullptr;
  size_t write_pos_ = 0;
  detail::monotonic_buffer_resource* resource_;
  linked_string_chunk* first_chunk_ = nullptr;
  linked_string_chunk* last_chunk_ = nullptr;
};

/// An output iterator that appends characters to a linked string chunk builder.
class linked_string_chunk_builder_output_iterator {
public:
  using difference_type = ptrdiff_t;
  using value_type = char;
  using pointer = char*;
  using reference = char&;
  using iterator_category = std::output_iterator_tag;

  explicit linked_string_chunk_builder_output_iterator(
    linked_string_chunk_builder* builder) noexcept
    : buiilder_(builder) {
    // nop
  }

  linked_string_chunk_builder_output_iterator& operator=(char ch) {
    buiilder_->append(ch);
    return *this;
  }

  linked_string_chunk_builder_output_iterator& operator*() {
    return *this;
  }

  linked_string_chunk_builder_output_iterator& operator++() {
    return *this;
  }

  linked_string_chunk_builder_output_iterator operator++(int) {
    return *this;
  }

private:
  linked_string_chunk_builder* buiilder_;
};

} // namespace caf

#ifdef CAF_USE_STD_FORMAT

#  include "caf/raise_error.hpp"

#  include <format>

namespace std {

template <>
struct std::formatter<caf::linked_string_chunk, char> {
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
  auto format(const caf::linked_string_chunk& head, FmtContext& ctx) const {
    auto out = ctx.out();
    return quoted ? head.copy_quoted_to(out) : head.copy_to(out);
  }
};

} // namespace std

#endif
