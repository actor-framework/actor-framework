/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
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

#ifndef CAF_INTRUSIVE_PTR_HPP
#define CAF_INTRUSIVE_PTR_HPP

#include <cstddef>
#include <algorithm>
#include <stdexcept>
#include <type_traits>

#include "caf/detail/comparable.hpp"

namespace caf {

/**
 * An intrusive, reference counting smart pointer impelementation.
 * @relates ref_counted
 */
template <class T>
class intrusive_ptr : detail::comparable<intrusive_ptr<T>>,
                      detail::comparable<intrusive_ptr<T>, const T*>,
                      detail::comparable<intrusive_ptr<T>, std::nullptr_t> {

 public:

  using pointer = T*;
  using const_pointer = const T*;
  using element_type = T;
  using reference = T&;
  using const_reference = const T&;

  constexpr intrusive_ptr() : m_ptr(nullptr) {
    // nop
  }

  intrusive_ptr(pointer raw_ptr, bool add_ref = true) {
    set_ptr(raw_ptr, add_ref);
  }

  intrusive_ptr(intrusive_ptr&& other) : m_ptr(other.release()) {
    // nop
  }

  intrusive_ptr(const intrusive_ptr& other) {
    set_ptr(other.get(), true);
  }

  template <class Y>
  intrusive_ptr(intrusive_ptr<Y> other) : m_ptr(other.release()) {
    static_assert(std::is_convertible<Y*, T*>::value,
            "Y* is not assignable to T*");
  }

  ~intrusive_ptr() {
    if (m_ptr) {
      m_ptr->deref();
    }
  }

  inline void swap(intrusive_ptr& other) {
    std::swap(m_ptr, other.m_ptr);
  }

  /**
   * Returns the raw pointer without modifying reference
   * count and sets this to `nullptr`.
   */
  pointer release() {
    auto result = m_ptr;
    m_ptr = nullptr;
    return result;
  }

  void reset(pointer new_value = nullptr, bool add_ref = true) {
    auto old = m_ptr;
    set_ptr(new_value, add_ref);
    if (old) {
      old->deref();
    }
  }

  template <class... Ts>
  void emplace(Ts&&... args) {
    reset(new T(std::forward<Ts>(args)...));
  }

  intrusive_ptr& operator=(pointer ptr) {
    reset(ptr);
    return *this;
  }

  intrusive_ptr& operator=(intrusive_ptr&& other) {
    swap(other);
    return *this;
  }

  intrusive_ptr& operator=(const intrusive_ptr& other) {
    intrusive_ptr tmp{other};
    swap(tmp);
    return *this;
  }

  template <class Y>
  intrusive_ptr& operator=(intrusive_ptr<Y> other) {
    intrusive_ptr tmp{std::move(other)};
    swap(tmp);
    return *this;
  }

  inline pointer get() const { return m_ptr; }
  inline pointer operator->() const { return m_ptr; }
  inline reference operator*() const { return *m_ptr; }

  inline bool operator!() const { return m_ptr == nullptr; }
  inline explicit operator bool() const { return m_ptr != nullptr; }

  inline ptrdiff_t compare(const_pointer ptr) const {
    return static_cast<ptrdiff_t>(get() - ptr);
  }

  inline ptrdiff_t compare(const intrusive_ptr& other) const {
    return compare(other.get());
  }

  inline ptrdiff_t compare(std::nullptr_t) const {
    return reinterpret_cast<ptrdiff_t>(get());
  }

  template <class C>
  intrusive_ptr<C> downcast() const {
    return (m_ptr) ? dynamic_cast<C*>(get()) : nullptr;
  }

  template <class C>
  intrusive_ptr<C> upcast() const {
    return (m_ptr) ? static_cast<C*>(get()) : nullptr;
  }

 private:

  pointer m_ptr;

  inline void set_ptr(pointer raw_ptr, bool add_ref) {
    m_ptr = raw_ptr;
    if (raw_ptr && add_ref) {
      raw_ptr->ref();
    }
  }
};

/**
 * @relates intrusive_ptr
 */
template <class X, typename Y>
bool operator==(const intrusive_ptr<X>& lhs, const intrusive_ptr<Y>& rhs) {
  return lhs.get() == rhs.get();
}

/**
 * @relates intrusive_ptr
 */
template <class X, typename Y>
bool operator!=(const intrusive_ptr<X>& lhs, const intrusive_ptr<Y>& rhs) {
  return !(lhs == rhs);
}

} // namespace caf

#endif // CAF_INTRUSIVE_PTR_HPP
