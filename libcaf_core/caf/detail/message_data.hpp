/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_DETAIL_MESSAGE_DATA_HPP
#define CAF_DETAIL_MESSAGE_DATA_HPP

#include <string>
#include <iterator>
#include <typeinfo>


#include "caf/fwd.hpp"
#include "caf/config.hpp"
#include "caf/ref_counted.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/type_erased_tuple.hpp"

#include "caf/detail/type_list.hpp"

namespace caf {
namespace detail {

class message_data : public ref_counted, public type_erased_tuple {
public:
  // -- nested types -----------------------------------------------------------

  class cow_ptr;

  // -- constructors, destructors, and assignment operators --------------------

  message_data() = default;
  message_data(const message_data&) = default;

  ~message_data() override;

  // -- pure virtual observers -------------------------------------------------

  virtual cow_ptr copy() const = 0;

  // -- observers --------------------------------------------------------------

  using type_erased_tuple::copy;

  bool shared() const noexcept override;
};

class message_data::cow_ptr {
public:
  // -- constructors, destructors, and assignment operators ------------------

  cow_ptr() noexcept = default;
  cow_ptr(cow_ptr&&) noexcept = default;
  cow_ptr(const cow_ptr&) noexcept = default;
  cow_ptr& operator=(cow_ptr&&) noexcept = default;
  cow_ptr& operator=(const cow_ptr&) noexcept = default;

  template <class T>
  cow_ptr(intrusive_ptr<T> p) noexcept : ptr_(std::move(p)) {
    // nop
  }

  inline cow_ptr(message_data* ptr, bool add_ref) noexcept
      : ptr_(ptr, add_ref) {
    // nop
  }

  // -- modifiers ------------------------------------------------------------

  inline void swap(cow_ptr& other) noexcept {
    ptr_.swap(other.ptr_);
  }

  inline void reset(message_data* p = nullptr, bool add_ref = true) noexcept {
    ptr_.reset(p, add_ref);
  }

  inline message_data* release() noexcept {
    return ptr_.detach();
  }

  inline void unshare() {
    static_cast<void>(get_unshared());
  }

  inline message_data* operator->() {
    return get_unshared();
  }

  inline message_data& operator*() {
    return *get_unshared();
  }

  /// Returns the raw pointer. Callers are responsible for unsharing
  /// the content if necessary.
  inline message_data* raw_ptr() {
    return ptr_.get();
  }

  // -- observers ------------------------------------------------------------

  inline const message_data* operator->() const noexcept {
    return ptr_.get();
  }

  inline const message_data& operator*() const noexcept {
    return *ptr_;
  }

  inline explicit operator bool() const noexcept {
    return static_cast<bool>(ptr_);
  }

  inline message_data* get() const noexcept {
    return ptr_.get();
  }

private:
  message_data* get_unshared();
  intrusive_ptr<message_data> ptr_;
};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_MESSAGE_DATA_HPP
