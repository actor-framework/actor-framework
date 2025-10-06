// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/config.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/implicit_conversions.hpp"
#include "caf/detail/padded_size.hpp"
#include "caf/fwd.hpp"
#include "caf/type_id_list.hpp"

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <new>

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

namespace caf::detail {

/// Container for storing an arbitrary number of message elements.
class CAF_CORE_EXPORT message_data {
public:
  // -- constructors, destructors, and assignment operators --------------------

  message_data() = delete;

  message_data(const message_data&) = delete;

  message_data& operator=(const message_data&) = delete;

  /// Constructs the message data object *without* constructing any element.
  explicit message_data(type_id_list types) noexcept;

  ~message_data() noexcept;

  message_data* copy() const;

  static intrusive_ptr<message_data> make_uninitialized(type_id_list types);

  // -- reference counting -----------------------------------------------------

  /// Increases reference count by one.
  void ref() const noexcept {
    rc_.fetch_add(1, std::memory_order_relaxed);
  }

  /// Decreases the reference count by one and destroys the object when its
  /// reference count drops to zero.
  void deref() noexcept {
    if (unique() || rc_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
      this->~message_data();
      free(const_cast<message_data*>(this));
    }
  }

  // -- properties -------------------------------------------------------------

  /// Queries whether there is exactly one reference to this data.
  bool unique() const noexcept {
    return rc_ == 1;
  }

  /// Returns the current number of references to this data.
  size_t get_reference_count() const noexcept {
    return rc_.load();
  }

  /// Returns the memory region for storing the message elements.
  std::byte* storage() noexcept {
    return storage_;
  }

  /// @copydoc storage
  const std::byte* storage() const noexcept {
    return storage_;
  }

  /// Returns the type IDs of the message elements.
  auto types() const noexcept {
    return types_;
  }

  /// Returns the number of elements.
  auto size() const noexcept {
    return types_.size();
  }

  /// Returns the memory location for the object at given index.
  /// @pre `index < size()`
  std::byte* at(size_t index) noexcept;

  /// @copydoc at
  const std::byte* at(size_t index) const noexcept;

  void inc_constructed_elements() {
    ++constructed_elements_;
  }

  template <class... Ts>
  void init(Ts&&... xs) {
    init_impl(storage(), std::forward<Ts>(xs)...);
  }

  std::byte* stepwise_init(std::byte* pos) {
    return pos;
  }

  template <class T, class... Ts>
  std::byte* stepwise_init(std::byte* pos, T&& x, Ts&&... xs) {
    using type = strip_and_convert_t<T>;
    new (pos) type(std::forward<T>(x));
    ++constructed_elements_;
    return stepwise_init(pos + padded_size_v<type>, std::forward<Ts>(xs)...);
  }

  std::byte* stepwise_init_from(std::byte* pos, const message& msg);

  std::byte* stepwise_init_from(std::byte* pos, const message_data* other);

  template <class Tuple, size_t... Is>
  std::byte*
  stepwise_init_from(std::byte* pos, Tuple&& tup, std::index_sequence<Is...>) {
    return stepwise_init(pos, std::get<Is>(std::forward<Tuple>(tup))...);
  }

  template <class... Ts>
  std::byte* stepwise_init_from(std::byte* pos, std::tuple<Ts...>&& tup) {
    return stepwise_init_from(pos, std::move(tup),
                              std::make_index_sequence<sizeof...(Ts)>{});
  }

  template <class... Ts>
  std::byte* stepwise_init_from(std::byte* pos, std::tuple<Ts...>& tup) {
    return stepwise_init_from(pos, tup,
                              std::make_index_sequence<sizeof...(Ts)>{});
  }

  template <class... Ts>
  std::byte* stepwise_init_from(std::byte* pos, const std::tuple<Ts...>& tup) {
    return stepwise_init_from(pos, tup,
                              std::make_index_sequence<sizeof...(Ts)>{});
  }

  template <class... Ts>
  void init_from(Ts&&... xs) {
    init_from_impl(storage(), std::forward<Ts>(xs)...);
  }

private:
  void init_impl(std::byte*) {
    // End of recursion.
  }

  template <class T, class... Ts>
  void init_impl(std::byte* storage, T&& x, Ts&&... xs) {
    using type = strip_and_convert_t<T>;
    new (storage) type(std::forward<T>(x));
    ++constructed_elements_;
    init_impl(storage + padded_size_v<type>, std::forward<Ts>(xs)...);
  }

  void init_from_impl(std::byte*) {
    // End of recursion.
  }

  template <class T, class... Ts>
  void init_from_impl(std::byte* pos, T&& x, Ts&&... xs) {
    init_from_impl(stepwise_init_from(pos, std::forward<T>(x)),
                   std::forward<Ts>(xs)...);
  }

  mutable std::atomic<size_t> rc_;
  type_id_list types_;
  size_t constructed_elements_;
  alignas(max_align_t) std::byte storage_[];
};

// -- related non-members ------------------------------------------------------

/// @relates message_data
inline void intrusive_ptr_add_ref(const message_data* ptr) {
  ptr->ref();
}

/// @relates message_data
inline void intrusive_ptr_release(message_data* ptr) {
  ptr->deref();
}

} // namespace caf::detail

#ifdef CAF_CLANG
#  pragma clang diagnostic pop
#elif defined(CAF_GCC)
#  pragma GCC diagnostic pop
#elif defined(CAF_MSVC)
#  pragma warning(pop)
#endif
