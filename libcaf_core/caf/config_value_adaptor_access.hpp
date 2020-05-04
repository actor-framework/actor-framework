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
#include "caf/config_value_object_access.hpp"
#include "caf/parser_state.hpp"
#include "caf/pec.hpp"
#include "caf/string_view.hpp"

namespace caf {

/// Enables user-defined types in config files and on the CLI by converting
/// them to and from tuples. Wraps a `config_value_object_access` in order to
/// allow CAF to interact with the underlying tuple.
///
/// ~~
/// struct trait {
///   using value_type = ...;
///
///   using tuple_type = ...;
///
///   static config_value_adaptor<...> adaptor_ref();
///
///   static span<config_value_field<object_type>*> fields();
///
///   static void convert(const value_type& src, tuple_type& dst);
///
///   static void convert(const tuple_type& src, value_type& dst);
/// };
/// ~~
template <class Trait>
struct config_value_adaptor_access {
  struct object_trait {
    using object_type = typename Trait::tuple_type;

    static std::string type_name() {
      return Trait::type_name();
    }

    static caf::span<config_value_field<object_type>*> fields() {
      return Trait::adaptor_ref().fields();
    }
  };

  using tuple_access = config_value_object_access<object_trait>;

  using value_type = typename Trait::value_type;

  using tuple_type = typename Trait::tuple_type;

  static std::string type_name() {
    return Trait::type_name();
  }

  static bool is(const config_value& x) {
    return tuple_access::is(x);
  }

  static optional<value_type> get_if(const config_value* x) {
    if (auto tmp = tuple_access::get_if(x)) {
      value_type result;
      convert(*tmp, result);
      return result;
    }
    return none;
  }

  static value_type get(const config_value& x) {
    auto tmp = tuple_access::get(x);
    value_type result;
    convert(tmp, result);
    return result;
  }

  template <class Nested>
  static void parse_cli(string_parser_state& ps, value_type& x, Nested nested) {
    tuple_type tmp;
    tuple_access::parse_cli(ps, tmp, nested);
    if (ps.code <= pec::trailing_character)
      convert(tmp, x);
  }

  static void convert(const value_type& src, tuple_type& dst) {
    Trait::convert(src, dst);
  }

  static void convert(const tuple_type& src, value_type& dst) {
    Trait::convert(src, dst);
  }

  static config_value::dictionary convert(const value_type& x) {
    tuple_type tmp;
    convert(x, tmp);
    return tuple_access::convert(tmp);
  }
};

} // namespace caf
