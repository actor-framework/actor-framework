// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/async/fwd.hpp"
#include "caf/byte_span.hpp"
#include "caf/config.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/raise_error.hpp"
#include "caf/type_id.hpp"

#include <atomic>

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

namespace caf {

/// An implicitly shared type for binary data.
class CAF_CORE_EXPORT chunk {
public:
  // -- member types -----------------------------------------------------------
  class data {
  public:
    data() = delete;

    data(const data&) = delete;

    data& operator=(const data&) = delete;

    data(bool is_bin, size_t size) : rc_(1), bin_(is_bin), size_(size) {
      static_cast<void>(padding_); // Silence unused-private-field warning.
    }

    explicit data(const_byte_span bytes);

    explicit data(std::string_view str);

    data(size_t total_size, span<const const_byte_span> bufs);

    data(size_t total_size, span<const std::string_view> bufs);

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
    std::byte padding_[padding_size];
    bool bin_;
    size_t size_;
    std::byte storage_[];
  };

  // -- constructors, destructors, and assignment operators --------------------

  chunk() noexcept = default;

  explicit chunk(const_byte_span buf);

  explicit chunk(intrusive_ptr<data> data) noexcept : data_(std::move(data)) {
    // nop
  }

  // -- factory functions ------------------------------------------------------

  template <class... ByteBuffers>
  static chunk from_buffers(const ByteBuffers&... buffers) {
    static_assert(sizeof...(ByteBuffers) > 0);
    const_byte_span bufs[sizeof...(ByteBuffers)] = {make_span(buffers)...};
    return chunk(make_span(bufs));
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

  void swap(chunk& other) {
    data_.swap(other.data_);
  }

  const_byte_span bytes() const noexcept {
    return data_ ? const_byte_span{data_->storage(), data_->size()}
                 : const_byte_span{};
  }

  const intrusive_ptr<data>& get_data() const& {
    return data_;
  }

  intrusive_ptr<data>&& get_data() && {
    return std::move(data_);
  }

private:
  explicit chunk(caf::span<const const_byte_span> bufs);

  template <class... Args>
  void init(size_t payload_size, Args&&... arg);

  intrusive_ptr<data> data_;
};

} // namespace caf

#ifdef CAF_CLANG
#  pragma clang diagnostic pop
#elif defined(CAF_GCC)
#  pragma GCC diagnostic pop
#elif defined(CAF_MSVC)
#  pragma warning(pop)
#endif
