// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/web_socket/frame.hpp"

#include "caf/async/fwd.hpp"
#include "caf/byte_span.hpp"
#include "caf/config.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/fwd.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/raise_error.hpp"
#include "caf/type_id.hpp"

namespace caf::net::lp {

/// An implicitly shared type for binary data frames.
class CAF_NET_EXPORT frame {
public:
  // -- constructors, destructors, and assignment operators --------------------

  frame() = default;

  frame(frame&&) = default;

  frame(const frame&) = default;

  frame& operator=(frame&&) = default;

  frame& operator=(const frame&) = default;

  explicit frame(const_byte_span buf);

  // -- factory functions ------------------------------------------------------

  template <class... ByteBuffers>
  static frame from_buffers(const ByteBuffers&... buffers) {
    static_assert(sizeof...(ByteBuffers) > 0);
    const_byte_span bufs[sizeof...(ByteBuffers)] = {make_span(buffers)...};
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

  const_byte_span bytes() const noexcept {
    return data_ ? const_byte_span{data_->storage(), data_->size()}
                 : const_byte_span{};
  }

private:
  explicit frame(caf::span<const const_byte_span> bufs);

  template <class... Args>
  void init(size_t payload_size, Args&&... arg);

  intrusive_ptr<web_socket::frame::data> data_;
};

} // namespace caf::net::lp
