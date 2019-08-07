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

#include "caf/detail/parser/state.hpp"
#include "caf/fwd.hpp"
#include "caf/string_view.hpp"

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

} // namespace detail
} // namespace caf
