/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include <atomic>
#include <cstdlib>

#include "caf/byte.hpp"
#include "caf/config.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/implicit_conversions.hpp"
#include "caf/detail/padded_size.hpp"
#include "caf/fwd.hpp"
#include "caf/type_id_list.hpp"

#ifdef CAF_CLANG
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wc99-extensions"
#elif defined(CAF_GCC)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wc99-extensions"
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
  explicit message_data(type_id_list types);

  ~message_data() noexcept;

  message_data* copy() const;

  // -- reference counting -----------------------------------------------------

  /// Increases reference count by one.
  void ref() const noexcept {
    rc_.fetch_add(1, std::memory_order_relaxed);
  }

  /// Decreases reference count by one and calls `request_deletion`
  /// when it drops to zero.
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
  byte* storage() noexcept {
    return storage_;
  }

  /// @copydoc storage
  const byte* storage() const noexcept {
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
  byte* at(size_t index) noexcept;

  /// @copydoc at
  const byte* at(size_t index) const noexcept;

  caf::error save(caf::serializer& sink) const;

  caf::error save(caf::binary_serializer& sink) const;

  caf::error load(caf::deserializer& source);

  caf::error load(caf::binary_deserializer& source);

private:
  mutable std::atomic<size_t> rc_;
  type_id_list types_;
  byte storage_[];
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

inline void message_data_init(byte*) {
  // nop
}

template <class T, class... Ts>
void message_data_init(byte* storage, T&& x, Ts&&... xs) {
  // TODO: exception safety: if any constructor throws, we need to unwind the
  //       stack here and call destructors.
  using type = strip_and_convert_t<T>;
  new (storage) strip_and_convert_t<T>(std::forward<T>(x));
  message_data_init(storage + padded_size_v<type>, std::forward<Ts>(xs)...);
}

} // namespace caf::detail

#ifdef CAF_CLANG
#  pragma clang diagnostic pop
#elif defined(CAF_GCC)
#  pragma GCC diagnostic pop
#endif
