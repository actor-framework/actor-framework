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
#include "caf/detail/dispatch_parse_cli.hpp"
#include "caf/detail/parse.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/optional.hpp"
#include "caf/string_view.hpp"

namespace caf {
namespace detail {

template <class MemberObjectPointer>
class config_value_field_impl;

// A config value with direct access to a field via member object pointer.
template <class Value, class Object>
class config_value_field_impl<Value Object::*>
  : public config_value_field<Object> {
public:
  using member_pointer = Value Object::*;

  using object_type = Object;

  using value_type = Value;

  using predicate_pointer = bool (*)(const value_type&);

  constexpr config_value_field_impl(string_view name, member_pointer ptr,
                                    optional<value_type> default_value = none,
                                    predicate_pointer predicate = nullptr)
    : name_(name),
      ptr_(ptr),
      default_value_(std::move(default_value)),
      predicate_(predicate) {
    // nop
  }

  constexpr config_value_field_impl(config_value_field_impl&&) = default;

  bool type_check(const config_value& x) const noexcept override {
    return holds_alternative<value_type>(x);
  }

  bool valid(const object_type& x) const noexcept override {
    return predicate_ ? predicate_(x.*ptr_) : true;
  }

  void set(object_type& x, const config_value& y) const override {
    x.*ptr_ = caf::get<value_type>(y);
  }

  config_value get(const object_type& x) const override {
    using access = select_config_value_access_t<value_type>;
    return config_value{access::convert(x.*ptr_)};
  }

  void parse_cli(string_parser_state& ps, object_type& x,
                 const char* char_blacklist) const override {
    detail::dispatch_parse_cli(ps, x.*ptr_, char_blacklist);
  }

  string_view name() const noexcept override {
    return name_;
  }

  bool has_default() const noexcept override {
    return static_cast<bool>(default_value_);
  }

  void set_default(object_type& x) const override {
    x.*ptr_ = *default_value_;
  }

private:
  string_view name_;
  member_pointer ptr_;
  optional<value_type> default_value_;
  predicate_pointer predicate_;
};

template <class Get>
struct config_value_field_base {
  using trait = get_callable_trait_t<Get>;

  static_assert(trait::num_args == 1,
                "Get must take exactly one argument (the object)");

  using arg_type = tl_head_t<typename trait::arg_types>;

  using value_type = decay_t<typename trait::result_type>;

  using type = config_value_field<decay_t<arg_type>>;
};

template <class Get>
using config_value_field_base_t = typename config_value_field_base<Get>::type;

// A config value with access to a field via getter and setter.
template <class Get, class Set>
class config_value_field_impl<std::pair<Get, Set>>
  : public config_value_field_base_t<Get> {
public:
  using super = config_value_field_base_t<Get>;

  using object_type = typename super::object_type;

  using get_trait = get_callable_trait_t<Get>;

  using value_type = decay_t<typename get_trait::result_type>;

  using predicate_pointer = bool (*)(const value_type&);

  constexpr config_value_field_impl(string_view name, Get getter, Set setter,
                                    optional<value_type> default_value = none,
                                    predicate_pointer predicate = nullptr)
    : name_(name),
      get_(std::move(getter)),
      set_(std::move(setter)),
      default_value_(std::move(default_value)),
      predicate_(predicate) {
    // nop
  }

  constexpr config_value_field_impl(config_value_field_impl&&) = default;

  bool type_check(const config_value& x) const noexcept override {
    return holds_alternative<value_type>(x);
  }

  void set(object_type& x, const config_value& y) const override {
    set_(x, caf::get<value_type>(y));
  }

  config_value get(const object_type& x) const override {
    using access = select_config_value_access_t<value_type>;
    return config_value{access::convert(get_(x))};
  }

  bool valid(const object_type& x) const noexcept override {
    return predicate_ ? predicate_(get_(x)) : true;
  }

  void parse_cli(string_parser_state& ps, object_type& x,
                 const char* char_blacklist) const override {
    value_type tmp;
    detail::dispatch_parse_cli(ps, tmp, char_blacklist);
    if (ps.code <= pec::trailing_character)
      set_(x, std::move(tmp));
  }

  string_view name() const noexcept override {
    return name_;
  }

  bool has_default() const noexcept override {
    return static_cast<bool>(default_value_);
  }

  void set_default(object_type& x) const override {
    set_(x, *default_value_);
  }

private:
  string_view name_;
  Get get_;
  Set set_;
  optional<value_type> default_value_;
  predicate_pointer predicate_;
};

} // namespace detail
} // namespace caf
