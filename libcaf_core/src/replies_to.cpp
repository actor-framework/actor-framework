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

#include "caf/replies_to.hpp"
#include "caf/string_algorithms.hpp"

namespace caf {

std::string replies_to_type_name(size_t input_size,
                                 const std::string* input,
                                 size_t output_opt1_size,
                                 const std::string* output_opt1) {
  using irange = iterator_range<const std::string*>;
  std::string glue = ",";
  std::string result;
  // 'void' is not an announced type, hence we check whether uniform_typeid
  // did return a valid pointer to identify 'void' (this has the
  // possibility of false positives, but those will be catched anyways)
  result = "caf::replies_to<";
  result += join(irange{input, input + input_size}, glue);
  result += ">::with<";
  result += join(irange{output_opt1, output_opt1 + output_opt1_size}, glue);
  result += ">";
  return result;
}

} // namespace caf
