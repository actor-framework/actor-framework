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

#include "caf/either.hpp"

#include "caf/to_string.hpp"
#include "caf/string_algorithms.hpp"

namespace caf {

std::string either_or_else_type_name(size_t lefts_size,
                                     const std::string* lefts,
                                     size_t rights_size,
                                     const std::string* rights) {
  std::string glue = ",";
  std::string result;
  result = "caf::either<";
  result += join(lefts, lefts + lefts_size, glue);
  result += ">::or_else<";
  result += join(rights, rights + rights_size, glue);
  result += ">";
  return result;
}

} // namespace caf
