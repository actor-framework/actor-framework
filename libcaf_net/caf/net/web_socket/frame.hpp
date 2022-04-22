// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/async/fwd.hpp"
#include "caf/byte_span.hpp"
#include "caf/config.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/fwd.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/raise_error.hpp"
#include "caf/type_id.hpp"

#include <atomic>
#include <cstddef>
#include <string_view>

#ifdef CAF_CLANG
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wc99-extensions"
#elif defined(CAF_GCC)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wpedantic"
#elif defined(CAF_MSVC)
#  pragma warning(push)
#  pragma warning(disable : 4200)
#endif

namespace caf::net::web_socket {

/// An implicitly shared type for passing along WebSocket data frames, i.e.,
/// text or binary frames.
class CAF_NET_EXPORT frame {
public:
  frame() = default;
  frame(frame&&) = default;
  frame(const frame&) = default;
  frame& operator=(frame&&) = default;
  frame& operator=(const frame&) = default;

  explicit frame(const_byte_span bytes);

  explicit frame(std::string_view text);

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

private:
  class data {
  public:
    data() = delete;

    data(const data&) = delete;

    data& operator=(const data&) = delete;

    explicit data(bool bin, size_t size) : rc_(1), bin_(bin), size_(size) {
      // nop
    }

    // -- reference counting ---------------------------------------------------

    bool unique() const noexcept {
      return rc_ == 1;
    }

    void ref() const noexcept {
      rc_.fetch_add(1, std::memory_order_relaxed);
    }

    void deref() noexcept;

    friend void intrusive_ptr_add_ref(const data* ptr) {
      ptr->ref();
    }

    friend void intrusive_ptr_release(data* ptr) {
      ptr->deref();
    }

    // -- properties -----------------------------------------------------------

    size_t size() noexcept {
      return size_;
    }

    bool is_binary() const noexcept {
      return bin_;
    }

    std::byte* storage() noexcept {
      return storage_;
    }

    const std::byte* storage() const noexcept {
      return storage_;
    }

  private:
    static constexpr size_t padding_size = CAF_CACHE_LINE_SIZE
                                           - sizeof(std::atomic<size_t>);
    mutable std::atomic<size_t> rc_;
    [[maybe_unused]] std::byte padding_[padding_size];
    bool bin_;
    size_t size_;
    std::byte storage_[];
  };

  explicit frame(intrusive_ptr<data> ptr) : data_(std::move(ptr)) {
    // nop
  }

  void alloc(bool is_binary, size_t num_bytes);

  intrusive_ptr<data> data_;
};

} // namespace caf::net::web_socket

#ifdef CAF_CLANG
#  pragma clang diagnostic pop
#elif defined(CAF_GCC)
#  pragma GCC diagnostic pop
#elif defined(CAF_MSVC)
#  pragma warning(pop)
#endif
