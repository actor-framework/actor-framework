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

#include "caf/config_value.hpp"
#include "caf/config_value_field.hpp"
#include "caf/detail/dispatch_parse_cli.hpp"
#include "caf/optional.hpp"

namespace caf {
namespace detail {

template <class Object, class Value>
class config_value_field_base : public config_value_field<Object> {
public:
  using super = config_value_field<Object>;

  using object_type = typename super::object_type;

  using value_type = Value;

  using predicate_type = bool (*)(const value_type&);

  config_value_field_base(string_view name, optional<value_type> default_value,
                          predicate_type predicate)
    : name_(name),
      default_value_(std::move(default_value)),
      predicate_(predicate) {
    // nop
  }

  config_value_field_base(config_value_field_base&&) = default;

  bool has_default() const noexcept override {
    return static_cast<bool>(default_value_);
  }

  string_view name() const noexcept override {
    return name_;
  }

  config_value get(const object_type& object) const override {
    using access = caf::select_config_value_access_t<value_type>;
    return config_value{access::convert(get_value(object))};
  }

  bool valid_input(const config_value& x) const override {
    if (!predicate_)
      return holds_alternative<value_type>(x);
    if (auto value = get_if<value_type>(&x))
      return predicate_(*value);
    return false;
  }

  bool set(object_type& x, const config_value& y) const override {
    if (auto value = get_if<value_type>(&y)) {
      if (predicate_ && !predicate_(*value))
        return false;
      set_value(x, move_if_optional(value));
      return true;
    }
    return false;
  }

  void set_default(object_type& x) const override {
    set_value(x, *default_value_);
  }

  void parse_cli(string_parser_state& ps, object_type& x,
                 const char* char_blacklist) const override {
    value_type tmp;
    dispatch_parse_cli(ps, tmp, char_blacklist);
    if (ps.code <= pec::trailing_character) {
      if (predicate_ && !predicate_(tmp))
        ps.code = pec::illegal_argument;
      else
        set_value(x, std::move(tmp));
    }
  }

  virtual const value_type& get_value(const object_type& object) const = 0;

  virtual void set_value(object_type& object, value_type value) const = 0;

protected:
  string_view name_;
  optional<value_type> default_value_;
  predicate_type predicate_;
};

} // namespace detail
} // namespace caf
