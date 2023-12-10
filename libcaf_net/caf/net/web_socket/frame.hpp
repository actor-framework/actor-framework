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
  // -- constructors, destructors, and assignment operators --------------------

  frame() = default;

  explicit frame(intrusive_ptr<chunk::data> ptr) : data_(std::move(ptr)) {
    // nop
  }

  explicit frame(const_byte_span buffer)
    : data_(chunk::data::make(buffer), false) {
    // nop
  }

  explicit frame(caf::span<const const_byte_span> buffers)
    : data_(chunk::data::make(buffers), false) {
    // nop
  }

  explicit frame(std::string_view text)
    : data_(chunk::data::make(text), false) {
    // nop
  }

  explicit frame(caf::span<const std::string_view> texts)
    : data_(chunk::data::make(texts), false) {
    // nop
  }

  // -- factory functions ------------------------------------------------------

  /// Creates a frame from a chunk.
  [[nodiscard]] static frame from_chunk(chunk ch) {
    return frame{std::move(ch).get_data()};
  }

  /// Creates a frame from one or more buffers.
  template <class... Buffers>
  [[nodiscard]] static frame from_buffers(const Buffers&... buffers) {
    static_assert(sizeof...(Buffers) > 0);
    const_byte_span bufs[sizeof...(Buffers)] = {make_span(buffers)...};
    return frame(make_span(bufs));
  }

  /// Creates a frame from one or more strings.
  template <class... Texts>
  [[nodiscard]] static frame from_strings(const Texts&... texts) {
    static_assert(sizeof...(Texts) > 0);
    std::string_view bufs[sizeof...(Texts)] = {std::string_view(texts)...};
    return frame(make_span(bufs));
  }

  // -- properties -------------------------------------------------------------

  /// Checks whether `get_data()` returns a non-null pointer.
  explicit operator bool() const noexcept {
    return static_cast<bool>(data_);
  }

  /// Returns the number of bytes stored in this frame.
  [[nodiscard]] size_t size() const noexcept {
    return data_ ? data_->size() : 0;
  }

  /// Returns whether `size() == 0`.
  [[nodiscard]] bool empty() const noexcept {
    return data_ ? data_->size() == 0 : true;
  }

  /// Exchange the contents of this frame with `other`.
  void swap(frame& other) noexcept {
    data_.swap(other.data_);
  }

  /// Returns the bytes stored in this frame.
  [[nodiscard]] const_byte_span bytes() const noexcept {
    return data_ ? const_byte_span{data_->storage(), data_->size()}
                 : const_byte_span{};
  }

  /// Returns the underlying data object.
  [[nodiscard]] const intrusive_ptr<chunk::data>& get_data() const& noexcept {
    return data_;
  }

  /// Returns the underlying data object.
  [[nodiscard]] intrusive_ptr<chunk::data>&& get_data() && noexcept {
    return std::move(data_);
  }

  /// Checks whether this frame contains binary data.
  [[nodiscard]] bool is_binary() const noexcept {
    return data_ && data_->is_binary();
  }

  /// Checks whether this frame contains text data.
  [[nodiscard]] bool is_text() const noexcept {
    return data_ && !data_->is_binary();
  }

  // -- conversions ------------------------------------------------------------

  /// Returns the bytes stored in this frame.
  [[nodiscard]] const_byte_span as_binary() const noexcept;

  /// Returns the characters stored in this frame.
  [[nodiscard]] std::string_view as_text() const noexcept;

  /// Converts this frame to a chunk.
  [[nodiscard]] chunk as_chunk() const& noexcept {
    return chunk{data_};
  }

  /// Converts this frame to a chunk.
  [[nodiscard]] chunk as_chunk() && noexcept {
    return chunk{std::move(data_)};
  }

private:
  intrusive_ptr<chunk::data> data_;
};

} // namespace caf::net::web_socket
