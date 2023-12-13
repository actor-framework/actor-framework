// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/chunked_string.hpp"

#include "caf/detail/monotonic_buffer_resource.hpp"

namespace caf {

namespace {

template <class T>
using allocator_t = detail::monotonic_buffer_resource::allocator<T>;

} // namespace

std::string to_string(const chunked_string& str) {
  std::string result;
  for (auto* chunk = str.head; chunk != nullptr; chunk = chunk->next)
    result.insert(result.end(), chunk->content.begin(), chunk->content.end());
  return result;
}

chunked_string_builder::chunked_string_builder(
  detail::monotonic_buffer_resource* resource)
  : resource_(resource) {
  auto* buf = allocator_t<chunked_string::node>{resource_}.allocate(1);
  first_chunk_ = new (buf) chunked_string::node();
  last_chunk_ = first_chunk_;
}

void chunked_string_builder::chunked_string_builder::append(char ch) {
  if (current_block_ == nullptr) { // Lazy initialization.
    current_block_ = allocator_t<char>{resource_}.allocate(chunk_size);
    write_pos_ = 0;
  } else if (write_pos_ == chunk_size) { // Flush current block if necessary.
    last_chunk_->content = std::string_view{current_block_, chunk_size};
    auto* buf = allocator_t<chunked_string::node>{resource_}.allocate(1);
    auto* next = new (buf) chunked_string::node();
    last_chunk_->next = next;
    last_chunk_ = next;
    current_block_ = allocator_t<char>{resource_}.allocate(chunk_size);
    write_pos_ = 0;
  }
  current_block_[write_pos_++] = ch;
}

chunked_string chunked_string_builder::seal() {
  if (current_block_ == nullptr)
    return chunked_string{first_chunk_};
  last_chunk_->content = std::string_view{current_block_, write_pos_};
  current_block_ = nullptr;
  return chunked_string{first_chunk_};
}

} // namespace caf
