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

#include <cstdint>
#include <iterator>
#include <utility>
#include <vector>

#include "caf/detail/parser/state.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/fwd.hpp"
#include "caf/string_view.hpp"
#include "caf/unit.hpp"

namespace caf {
namespace detail {

using parse_state = parser::state<string_view::iterator>;

// -- signed integer types -----------------------------------------------------

void parse(parse_state& ps, int8_t& x);

void parse(parse_state& ps, int16_t& x);

void parse(parse_state& ps, int32_t& x);

void parse(parse_state& ps, int64_t& x);

// -- unsigned integer types ---------------------------------------------------

void parse(parse_state& ps, uint8_t& x);

void parse(parse_state& ps, uint16_t& x);

void parse(parse_state& ps, uint32_t& x);

void parse(parse_state& ps, uint64_t& x);

// -- floating point types -----------------------------------------------------

void parse(parse_state& ps, float& x);

void parse(parse_state& ps, double& x);

// -- CAF types ----------------------------------------------------------------

void parse(parse_state& ps, atom_value& x);

void parse(parse_state& ps, uri& x);

// -- STL types ----------------------------------------------------------------

void parse(parse_state& ps, std::string& x);

// -- container types ----------------------------------------------------------

template <class First, class Second>
void parse(parse_state& ps, std::pair<First, Second>& kvp) {
  parse(ps, kvp.first);
  if (ps.code > pec::trailing_character)
    return;
  if (!ps.consume('=')) {
    ps.code = pec::unexpected_character;
    return;
  }
  parse(ps, kvp.second);
}

template <class T>
enable_if_tt<is_iterable<T>> parse(parse_state& ps, T& xs) {
  using value_type = deconst_kvp_pair_t<typename T::value_type>;
  static constexpr auto is_map_type = is_pair<value_type>::value;
  static constexpr auto opening_char = is_map_type ? '{' : '[';
  static constexpr auto closing_char = is_map_type ? '}' : ']';
  auto out = std::inserter(xs, xs.end());
  if (ps.consume(opening_char)) {
    do {
      if (ps.consume(closing_char)) {
        ps.skip_whitespaces();
        ps.code = ps.at_end() ? pec::success : pec::trailing_character;
        return;
      }
      value_type tmp;
      parse(ps, tmp);
      if (ps.code > pec::trailing_character)
        return;
      *out++ = std::move(tmp);
    } while (ps.consume(','));
    if (ps.consume(closing_char)) {
      ps.skip_whitespaces();
      ps.code = ps.at_end() ? pec::success : pec::trailing_character;
    } else {
      ps.code = pec::unexpected_character;
    }
    return;
  }
  do {
    value_type tmp;
    parse(ps, tmp);
    if (ps.code > pec::trailing_character)
      return;
    *out++ = std::move(tmp);
  } while (ps.consume(','));
  ps.code = ps.at_end() ? pec::success : pec::trailing_character;
}

} // namespace detail
} // namespace caf
