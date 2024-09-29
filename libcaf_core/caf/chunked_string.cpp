// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/chunked_string.hpp"

#include "caf/detail/monotonic_buffer_resource.hpp"

#include <numeric>
#include <ostream>

namespace caf {

namespace {

template <class T>
using allocator_t = detail::monotonic_buffer_resource::allocator<T>;

} // namespace

size_t chunked_string::size() const noexcept {
  auto fn = [](size_t acc, std::string_view chunk) {
    return acc + chunk.size();
  };
  return std::accumulate(begin(), end(), size_t{0}, fn);
}

std::string to_string(const chunked_string& str) {
  std::string result;
  result.reserve(str.size());
  for (auto chunk : str)
    result.insert(result.end(), chunk.begin(), chunk.end());
  return result;
}

std::ostream& operator<<(std::ostream& out, const chunked_string& str) {
  for (auto chunk : str)
    out << chunk;
  return out;
}

chunked_string_builder::chunked_string_builder(
  resource_type* resource) noexcept {
  new (&chunks_) list_type(resource);
}

void chunked_string_builder::append(char ch) {
  if (current_block_ == nullptr) { // Lazy initialization.
    current_block_ = allocator_t<char>{resource()}.allocate(chunk_size);
    write_pos_ = 0;
  } else if (write_pos_ == chunk_size) { // Flush current block if necessary.
    auto& back = chunks_.emplace_back();
    back = std::string_view{current_block_, chunk_size};
    current_block_ = allocator_t<char>{resource()}.allocate(chunk_size);
    write_pos_ = 0;
  }
  current_block_[write_pos_++] = ch;
}

chunked_string chunked_string_builder::build() {
  if (current_block_ != nullptr) {
    auto& back = chunks_.emplace_back();
    back = std::string_view{current_block_, write_pos_};
  }
  return chunked_string{chunks_.head()};
}

} // namespace caf
