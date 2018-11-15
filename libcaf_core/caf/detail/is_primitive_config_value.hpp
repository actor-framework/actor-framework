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

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "caf/detail/is_one_of.hpp"
#include "caf/timespan.hpp"

// -- forward declarations (this header cannot include fwd.hpp) ----------------

namespace caf {

class config_value;
enum class atom_value : uint64_t;

} // namespace caf

// -- trait --------------------------------------------------------------------

namespace caf {
namespace detail {

/// Checks wheter `T` is in a primitive value type in `config_value`.
template <class T>
using is_primitive_config_value =
  is_one_of<T, int64_t, bool, double, atom_value, timespan, std::string,
            std::vector<config_value>, std::map<std::string, config_value>>;

} // namespace detail
} // namespace caf

