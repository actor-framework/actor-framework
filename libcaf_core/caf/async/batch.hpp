// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>

#include "caf/async/fwd.hpp"
#include "caf/byte.hpp"
#include "caf/config.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/raise_error.hpp"
#include "caf/span.hpp"
#include "caf/type_id.hpp"

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

namespace caf::async {

/// A reference-counted container for transferring items from publishers to
/// subscribers.
class CAF_CORE_EXPORT batch {
public:
  template <class T>
  friend batch make_batch(span<const T> items);

  using item_destructor = void (*)(type_id_t, uint16_t, size_t, byte*);

  batch() = default;
  batch(batch&&) = default;
  batch(const batch&) = default;
  batch& operator=(batch&&) = default;
  batch& operator=(const batch&) = default;

  size_t size() const noexcept {
    return data_ ? data_->size() : 0;
  }

  bool empty() const noexcept {
    return data_ ? data_->size() == 0 : true;
  }

  template <class T>
  span<T> items() noexcept {
    return data_ ? data_->items<T>() : span<T>{};
  }

  template <class T>
  span<const T> items() const noexcept {
    return data_ ? data_->items<T>() : span<const T>{};
  }

  bool save(serializer& f) const;

  bool save(binary_serializer& f) const;

  bool load(deserializer& f);

  bool load(binary_deserializer& f);

  void swap(batch& other) {
    data_.swap(other.data_);
  }

private:
  template <class Inspector>
  bool save_impl(Inspector& f) const;

  template <class Inspector>
  bool load_impl(Inspector& f);

  class data {
  public:
    data() = delete;

    data(const data&) = delete;

    data& operator=(const data&) = delete;

    data(item_destructor destroy_items, type_id_t item_type, uint16_t item_size,
         size_t size)
      : rc_(1),
        destroy_items_(destroy_items),
        item_type_(item_type),
        item_size_(item_size),
        size_(size) {
      // nop
    }

    ~data() {
      if (size_ > 0)
        destroy_items_(item_type_, item_size_, size_, storage_);
    }

    // -- reference counting ---------------------------------------------------

    bool unique() const noexcept {
      return rc_ == 1;
    }

    void ref() const noexcept {
      rc_.fetch_add(1, std::memory_order_relaxed);
    }

    void deref() noexcept {
      if (unique() || rc_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        this->~data();
        free(this);
      }
    }

    friend void intrusive_ptr_add_ref(const data* ptr) {
      ptr->ref();
    }

    friend void intrusive_ptr_release(data* ptr) {
      ptr->deref();
    }

    // -- properties -----------------------------------------------------------

    type_id_t item_type() {
      return item_type_;
    }

    size_t size() noexcept {
      return size_;
    }

    byte* storage() noexcept {
      return storage_;
    }

    const byte* storage() const noexcept {
      return storage_;
    }

    template <class T>
    span<T> items() noexcept {
      return {reinterpret_cast<T*>(storage_), size_};
    }

    template <class T>
    span<const T> items() const noexcept {
      return {reinterpret_cast<const T*>(storage_), size_};
    }

    // -- serialization --------------------------------------------------------

    /// @pre `size() > 0`
    template <class Inspector>
    bool save(Inspector& sink) const;

    template <class Inspector>
    bool load(Inspector& source);

  private:
    mutable std::atomic<size_t> rc_;
    item_destructor destroy_items_;
    type_id_t item_type_;
    uint16_t item_size_;
    size_t size_;
    byte storage_[];
  };

  explicit batch(intrusive_ptr<data> ptr) : data_(std::move(ptr)) {
    // nop
  }

  intrusive_ptr<data> data_;
};

template <class Inspector>
auto inspect(Inspector& f, batch& x)
  -> std::enable_if_t<!Inspector::is_loading, decltype(x.save(f))> {
  return x.save(f);
}

template <class Inspector>
auto inspect(Inspector& f, batch& x)
  -> std::enable_if_t<Inspector::is_loading, decltype(x.load(f))> {
  return x.load(f);
}

template <class T>
batch make_batch(span<const T> items) {
  static_assert(sizeof(T) < 0xFFFF);
  auto total_size = sizeof(batch::data) + (items.size() * sizeof(T));
  auto vptr = malloc(total_size);
  if (vptr == nullptr)
    CAF_RAISE_ERROR(std::bad_alloc, "make_batch");
  auto destroy_items = [](type_id_t, uint16_t, size_t size, byte* storage) {
    auto ptr = reinterpret_cast<T*>(storage);
    std::destroy(ptr, ptr + size);
  };
  intrusive_ptr<batch::data> ptr{
    new (vptr) batch::data(destroy_items, type_id_or_invalid<T>(),
                           static_cast<uint16_t>(sizeof(T)), items.size()),
    false};
  std::uninitialized_copy(items.begin(), items.end(),
                          reinterpret_cast<T*>(ptr->storage()));
  return batch{std::move(ptr)};
}

} // namespace caf::async

#ifdef CAF_CLANG
#  pragma clang diagnostic pop
#elif defined(CAF_GCC)
#  pragma GCC diagnostic pop
#elif defined(CAF_MSVC)
#  pragma warning(pop)
#endif
