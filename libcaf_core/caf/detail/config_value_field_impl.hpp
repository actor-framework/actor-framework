/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
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

#include <string>
#include <utility>

#include "caf/config_value.hpp"
#include "caf/config_value_field.hpp"
#include "caf/detail/config_value_field_base.hpp"
#include "caf/detail/dispatch_parse_cli.hpp"
#include "caf/detail/parse.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/optional.hpp"
#include "caf/string_view.hpp"

namespace caf::detail {

template <class MemberObjectPointer>
class config_value_field_impl;

// A config value with direct access to a field via member object pointer.
template <class Value, class Object>
class config_value_field_impl<Value Object::*>
  : public config_value_field_base<Object, Value> {
public:
  using super = config_value_field_base<Object, Value>;

  using member_pointer = Value Object::*;

  using object_type = Object;

  using value_type = Value;

  using predicate_type = bool (*)(const value_type&);

  constexpr config_value_field_impl(string_view name, member_pointer ptr,
                                    optional<value_type> default_value = none,
                                    predicate_type predicate = nullptr)
    : super(name, std::move(default_value), predicate), ptr_(ptr) {
    // nop
  }

  constexpr config_value_field_impl(config_value_field_impl&&) = default;

  const value_type& get_value(const object_type& x) const override {
    return x.*ptr_;
  }

  void set_value(object_type& x, value_type y) const override {
    x.*ptr_ = std::move(y);
  }

private:
  member_pointer ptr_;
};

template <class Get>
struct config_value_field_trait {
  using trait = get_callable_trait_t<Get>;

  static_assert(trait::num_args == 1,
                "Get must take exactly one argument (the object)");

  using get_argument_type = tl_head_t<typename trait::arg_types>;

  using object_type = decay_t<get_argument_type>;

  using get_result_type = typename trait::result_type;

  using value_type = decay_t<get_result_type>;
};

// A config value with access to a field via getter and setter.
template <class Get, class Set>
class config_value_field_impl<std::pair<Get, Set>>

  : public config_value_field_base<
      typename config_value_field_trait<Get>::object_type,
      typename config_value_field_trait<Get>::value_type> {
public:
  using trait = config_value_field_trait<Get>;

  using object_type = typename trait::object_type;

  using get_result_type = typename trait::get_result_type;

  using value_type = typename trait::value_type;

  using predicate_type = bool (*)(const value_type&);

  using super = config_value_field_base<object_type, value_type>;

  constexpr config_value_field_impl(string_view name, Get getter, Set setter,
                                    optional<value_type> default_value = none,
                                    predicate_type predicate = nullptr)
    : super(name, std::move(default_value), predicate),
      get_(std::move(getter)),
      set_(std::move(setter)) {
    // nop
  }

  constexpr config_value_field_impl(config_value_field_impl&&) = default;

  const value_type& get_value(const object_type& x) const override {
    bool_token<std::is_lvalue_reference<get_result_type>::value> token;
    return get_value_impl(x, token);
  }

  void set_value(object_type& x, value_type y) const override {
    set_(x, std::move(y));
  }

private:
  template <class O>
  const value_type& get_value_impl(const O& x, std::true_type) const {
    return get_(x);
  }

  template <class O>
  const value_type& get_value_impl(const O& x, std::false_type) const {
    dummy_ = get_(x);
    return dummy_;
  }

  Get get_;
  Set set_;
  mutable value_type dummy_;
};

} // namespace caf
