/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2020 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/type_id_list.hpp"

#include "caf/detail/meta_object.hpp"

namespace caf {

std::string to_string(type_id_list xs) {
  if (!xs || xs.size() == 0)
    return "[]";
  std::string result;
  result += '[';
  {
    auto tn = detail::global_meta_object(xs[0])->type_name;
    result.insert(result.end(), tn.begin(), tn.end());
  }
  for (size_t index = 1; index < xs.size(); ++index) {
    result += ", ";
    auto tn = detail::global_meta_object(xs[index])->type_name;
    result.insert(result.end(), tn.begin(), tn.end());
  }
  result += ']';
  return result;
}

} // namespace caf
