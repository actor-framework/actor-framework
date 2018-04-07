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

#include <type_traits>

#include "caf/atom.hpp"

namespace caf {

/// Represents a message handler parameter of type `T` and
/// guarantees copy-on-write semantics.
template <class T>
class param {
public:
  enum flag {
    shared_access,    // x_ lives in a shared type_erased_tuple
    exclusive_access, // x_ lives in an unshared type_erased_tuple
    private_access    // x_ is a copy of the original value
  };

  param(const void* ptr, bool is_shared)
      : x_(reinterpret_cast<T*>(const_cast<void*>(ptr))) {
    flag_ = is_shared ? shared_access : exclusive_access;
  }

  param(const param& other) = delete;
  param& operator=(const param& other) = delete;

  param(param&& other) : x_(other.x_), flag_(other.flag_) {
    other.x_ = nullptr;
  }

  ~param() {
    if (flag_ == private_access)
      delete x_;
  }

  const T& get() const {
    return *x_;
  }

  operator const T&() const {
    return *x_;
  }

  const T* operator->() const {
    return x_;
  }

  /// Detaches the value if needed and returns a mutable reference to it.
  T& get_mutable() {
    if (flag_ == shared_access) {
      auto cpy = new T(get());
      x_ = cpy;
      flag_ = private_access;
    }
    return *x_;
  }

  /// Moves the value out of the `param`.
  T&& move() {
    return std::move(get_mutable());
  }

private:
  T* x_;
  flag flag_;
};

/// Convenience alias that wraps `T` into `param<T>`
/// unless `T` is arithmetic or an atom constant.
template <class T>
using param_t =
  typename std::conditional<
    std::is_arithmetic<T>::value || is_atom_constant<T>::value,
    T,
    param<T>
  >::type;

/// Unpacks `param<T>` to `T`.
template <class T>
struct remove_param {
  using type = T;
};

template <class T>
struct remove_param<param<T>> {
  using type = T;
};

/// Convenience struct for `remove_param<std::decay<T>>`.
template <class T>
struct param_decay {
  using type = typename remove_param<typename std::decay<T>::type>::type;
};

} // namespace caf

