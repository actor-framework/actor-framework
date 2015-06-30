/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
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

#ifndef CAF_ABSTRACT_UNIFORM_TYPE_INFO_HPP
#define CAF_ABSTRACT_UNIFORM_TYPE_INFO_HPP

#include "caf/message.hpp"
#include "caf/deserializer.hpp"
#include "caf/uniform_type_info.hpp"

#include "caf/detail/type_traits.hpp"

namespace caf {

/// Implements all pure virtual functions of `uniform_type_info`
/// except serialize() and deserialize().
template <class T>
class abstract_uniform_type_info : public uniform_type_info {
public:
  const char* name() const override {
    return name_.c_str();
  }

  message as_message(void* instance) const override {
    return make_message(deref(instance));
  }

  bool equal_to(const std::type_info& tinfo) const override {
    return native_ == &tinfo || *native_ == tinfo;
  }

  bool equals(const void* lhs, const void* rhs) const override {
    return eq(deref(lhs), deref(rhs));
  }

  uniform_value create(const uniform_value& other) const override {
    return create_impl<T>(other);
  }

protected:
  abstract_uniform_type_info(std::string tname)
      : name_(std::move(tname)),
        native_(&typeid(T)) {
    // nop
  }

  static const T& deref(const void* ptr) {
    return *reinterpret_cast<const T*>(ptr);
  }

  static T& deref(void* ptr) {
    return *reinterpret_cast<T*>(ptr);
  }

  // can be overridden in subclasses to compare POD types
  // by comparing each individual member
  virtual bool pod_mems_equals(const T&, const T&) const {
    return false;
  }

  std::string name_;
  const std::type_info* native_;

private:
  template <class C>
  typename std::enable_if<std::is_empty<C>::value, bool>::type
  eq(const C&, const C&) const {
    return true;
  }

  template <class C>
  typename std::enable_if<
    ! std::is_empty<C>::value && detail::is_comparable<C, C>::value,
    bool
  >::type
  eq(const C& lhs, const C& rhs) const {
    return lhs == rhs;
  }

  template <class C>
  typename std::enable_if<
    ! std::is_empty<C>::value
    && ! detail::is_comparable<C, C>::value
    && std::is_pod<C>::value,
    bool
  >::type
  eq(const C& lhs, const C& rhs) const {
    return pod_mems_equals(lhs, rhs);
  }
};

} // namespace caf

#endif // CAF_ABSTRACT_UNIFORM_TYPE_INFO_HPP
