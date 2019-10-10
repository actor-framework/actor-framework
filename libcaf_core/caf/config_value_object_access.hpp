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

namespace caf {

/// Enables user-defined types in config files and on the CLI by converting
/// them to and from `config_value::dictionary`.
///
/// ~~
/// struct trait {
///   using object_type = ...;
///
///   static string_value type_name();
///
///   static span<config_value_field<object_type>*> fields();
/// };
/// ~~
template <class Trait>
struct config_value_object_access {
  using object_type = typename Trait::object_type;

  static std::string type_name() {
    return Trait::type_name();
  }

  static bool extract(const config_value* src, object_type* dst) {
    auto dict = caf::get_if<config_value::dictionary>(src);
    if (!dict)
      return false;
    for (auto field : Trait::fields()) {
      auto value = caf::get_if(dict, field->name());
      if (!value) {
        if (!field->has_default())
          return false;
        if (dst)
          field->set_default(*dst);
      } else if (field->type_check(*value)) {
        if (dst)
          field->set(*dst, *value);
      } else {
        return false;
      }
    }
    return true;
  }

  static bool is(const config_value& x) {
    return extract(&x, nullptr);
  }

  static optional<object_type> get_if(const config_value* x) {
    object_type result;
    if (extract(x, &result))
      return result;
    return none;
  }

  static object_type get(const config_value& x) {
    auto result = get_if(&x);
    if (!result)
      CAF_RAISE_ERROR("config_value does not contain requested object");
    return std::move(*result);
  }

  static config_value::dictionary convert(const object_type& x) {
    config_value::dictionary result;
    for (auto field : Trait::fields())
      result.emplace(field->name(), field->get(x));
    return result;
  }

  static void parse_cli(string_parser_state& ps, object_type& x) {
    using field_type = config_value_field<object_type>;
    std::vector<field_type*> parsed_fields;
    auto got = [&](field_type* f) {
      auto e = parsed_fields.end();
      return std::find(parsed_fields.begin(), e, f) != e;
    };
    auto push = [&](field_type* f) {
      if (got(f))
        return false;
      parsed_fields.emplace_back(f);
      return true;
    };
    auto finalize = [&] {
      for (auto field : Trait::fields()) {
        if (!got(field)) {
          if (field->has_default()) {
            field->set_default(x);
          } else {
            ps.code = pec::missing_field;
            return;
          }
        }
      }
      ps.skip_whitespaces();
      ps.code = ps.at_end() ? pec::success : pec::trailing_character;
    };
    auto fs = Trait::fields();
    if (!ps.consume('{')) {
      ps.code = pec::unexpected_character;
      return;
    }
    using string_access = select_config_value_access_t<std::string>;
    config_value::dictionary result;
    do {
      if (ps.consume('}')) {
        finalize();
        return;
      }
      std::string field_name;
      string_access::parse_cli(ps, field_name, "=}");
      if (ps.code > pec::trailing_character)
        return;
      if (!ps.consume('=')) {
        ps.code = ps.at_end() ? pec::unexpected_eof : pec::unexpected_character;
        return;
      }
      auto predicate = [&](config_value_field<object_type>* x) {
        return x->name() == field_name;
      };
      auto f = std::find_if(fs.begin(), fs.end(), predicate);
      if (f == fs.end()) {
        ps.code = pec::invalid_field_name;
        return;
      }
      auto fptr = *f;
      if (!push(fptr)) {
        ps.code = pec::repeated_field_name;
        return;
      }
      fptr->parse_cli(ps, x, ",}");
      if (ps.code > pec::trailing_character)
        return;
      if (ps.at_end()) {
        ps.code = pec::unexpected_eof;
        return;
      }
      if (!fptr->valid(x)) {
        ps.code = pec::illegal_argument;
        return;
      }
      result[fptr->name()] = fptr->get(x);
    } while (ps.consume(','));
    if (!ps.consume('}')) {
      ps.code = ps.at_end() ? pec::unexpected_eof : pec::unexpected_character;
      return;
    }
    finalize();
  }
};

} // namespace caf
