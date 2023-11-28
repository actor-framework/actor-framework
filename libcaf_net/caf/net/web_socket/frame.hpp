// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/async/fwd.hpp"
#include "caf/byte_span.hpp"
#include "caf/chunk.hpp"
#include "caf/config.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/fwd.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/raise_error.hpp"
#include "caf/type_id.hpp"

#include <atomic>
#include <cstddef>
#include <string_view>

namespace caf::net::web_socket {

/// An implicitly shared type for passing along WebSocket data frames, i.e.,
/// text or binary frames.
class CAF_NET_EXPORT frame {
public:
  // -- factory functions ------------------------------------------------------

  static frame from_chunk(chunk ch) {
    return frame{std::move(ch).get_data()};
  }

  // -- constructors, destructors, and assignment operators --------------------

  frame() = default;

  frame(frame&&) = default;

  frame(const frame&) = default;

  frame& operator=(frame&&) = default;

  frame& operator=(const frame&) = default;

  explicit frame(const_byte_span bytes) : data_(chunk{bytes}.get_data()) {
    // nop
  }

  explicit frame(std::string_view text);

  // -- factory functions ------------------------------------------------------

  template <class... Buffers>
  static frame from_buffers(const Buffers&... buffers) {
    static_assert(sizeof...(Buffers) > 0);
    const_byte_span bufs[sizeof...(Buffers)] = {make_span(buffers)...};
    return frame(make_span(bufs));
  }

  template <class... Buffers>
  static frame from_strings(const Buffers&... buffers) {
    static_assert(sizeof...(Buffers) > 0);
    std::string_view bufs[sizeof...(Buffers)] = {std::string_view(buffers)...};
    return frame(make_span(bufs));
  }

  // -- properties -------------------------------------------------------------

  explicit operator bool() const noexcept {
    return static_cast<bool>(data_);
  }

  size_t size() const noexcept {
    return data_ ? data_->size() : 0;
  }

  bool empty() const noexcept {
    return data_ ? data_->size() == 0 : true;
  }

  void swap(frame& other) {
    data_.swap(other.data_);
  }

  bool is_binary() const noexcept {
    return data_ && data_->is_binary();
  }

  bool is_text() const noexcept {
    return data_ && !data_->is_binary();
  }

  const_byte_span as_binary() const noexcept;

  std::string_view as_text() const noexcept;

  chunk as_chunk() const& {
    return chunk{data_};
  }

  chunk as_chunk() && {
    return chunk{std::move(data_)};
  }

private:
  explicit frame(intrusive_ptr<chunk::data> ptr) : data_(std::move(ptr)) {
    // nop
  }

  explicit frame(caf::span<const const_byte_span> buffers);

  explicit frame(caf::span<const std::string_view> buffers);

  template <class... Args>
  void init(size_t payload_size, Args&&... arg);

  intrusive_ptr<chunk::data> data_;
};

} // namespace caf::net::web_socket
