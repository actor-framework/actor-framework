// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/async/fwd.hpp"
#include "caf/config.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/raise_error.hpp"
#include "caf/span.hpp"
#include "caf/type_id.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>

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

/// A reference-counted, type-erased container for transferring items from
/// producers to consumers.
class CAF_CORE_EXPORT batch {
public:
  using item_destructor = void (*)(type_id_t, size_t, size_t, std::byte*);

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

  type_id_t item_type() const noexcept {
    return data_ ? data_->item_type_ : invalid_type_id;
  }

  template <class T>
  span<const T> items() const {
    if (item_type() == type_id_v<T>)
      return data_->items<T>();
    return span<const T>{};
  }

  bool save(serializer& f) const;

  bool save(binary_serializer& f) const;

  bool load(deserializer& f);

  bool load(binary_deserializer& f);

  void swap(batch& other) {
    data_.swap(other.data_);
  }

  template <class List>
  static batch from(const List& items) {
    if (items.empty())
      return {};
    using value_type = typename List::value_type;
    static_assert(sizeof(value_type) < 0xFFFF);
    auto total_size = sizeof(batch::data) + (items.size() * sizeof(value_type));
    auto vptr = malloc(total_size);
    if (vptr == nullptr)
      CAF_RAISE_ERROR(std::bad_alloc, "failed to allocate memory for batch");
    auto destroy_items = [](type_id_t, size_t, size_t size,
                            std::byte* storage) {
      auto ptr = reinterpret_cast<value_type*>(storage);
      std::destroy(ptr, ptr + size);
    };
    // We start the item count at 0 and increment it for each successfully
    // loaded item. This makes sure that the destructor only destroys fully
    // constructed items in case of an error or exception.
    intrusive_ptr<batch::data> ptr{
      new (vptr) batch::data(destroy_items, type_id_or_invalid<value_type>(),
                             sizeof(value_type), 0),
      false};
    auto* storage = ptr->storage_;
    for (const auto& item : items) {
      new (storage) value_type(item);
      ++ptr->size_;
      storage += sizeof(value_type);
    }
    return batch{std::move(ptr)};
  }

private:
  template <class Inspector>
  bool save_impl(Inspector& f) const;

  template <class Inspector>
  bool load_impl(Inspector& f);

  class data {
  public:
    friend class batch;

    data() = delete;

    data(const data&) = delete;

    data& operator=(const data&) = delete;

    data(item_destructor destroy_items, type_id_t item_type, size_t item_size,
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

    type_id_t item_type() const noexcept {
      return item_type_;
    }

    size_t size() const noexcept {
      return size_;
    }

    const std::byte* storage() const noexcept {
      return storage_;
    }

    template <class T>
    span<const T> items() const noexcept {
      return {reinterpret_cast<const T*>(storage_), size_};
    }

    // -- serialization --------------------------------------------------------

    /// @pre `size() > 0`
    template <class Inspector>
    bool save(Inspector& sink) const;

  private:
    mutable std::atomic<size_t> rc_;
    item_destructor destroy_items_;
    type_id_t item_type_;
    size_t item_size_;
    size_t size_;
    std::byte storage_[];
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

template <class List>
batch make_batch(const List& items) {
  return batch::from(items);
}

} // namespace caf::async

#ifdef CAF_CLANG
#  pragma clang diagnostic pop
#elif defined(CAF_GCC)
#  pragma GCC diagnostic pop
#elif defined(CAF_MSVC)
#  pragma warning(pop)
#endif

namespace caf::detail {

template <class T>
class unbatch {
public:
  using input_type = async::batch;

  using output_type = T;

  template <class Next, class... Steps>
  bool on_next(const async::batch& xs, Next& next, Steps&... steps) {
    for (const auto& item : xs.template items<T>())
      if (!next.on_next(item, steps...))
        return false;
    return true;
  }

  template <class Next, class... Steps>
  void on_complete(Next& next, Steps&... steps) {
    next.on_complete(steps...);
  }

  template <class Next, class... Steps>
  void on_error(const error& what, Next& next, Steps&... steps) {
    next.on_error(what, steps...);
  }
};

template <class T>
struct batching_trait {
  static constexpr bool skip_empty = true;
  using input_type = T;
  using output_type = async::batch;
  using select_token_type = int64_t;

  output_type operator()(const std::vector<input_type>& xs) {
    return async::make_batch(make_span(xs));
  }
};

} // namespace caf::detail
