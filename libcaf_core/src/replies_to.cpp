// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/replies_to.hpp"
#include "caf/string_algorithms.hpp"

namespace caf {

std::string replies_to_type_name(size_t input_size, const std::string* input,
                                 size_t output_opt1_size,
                                 const std::string* output_opt1) {
  std::string_view glue = ",";
  std::string result;
  // 'void' is not an announced type, hence we check whether uniform_typeid
  // did return a valid pointer to identify 'void' (this has the
  // possibility of false positives, but those will be caught anyways)
  result = "caf::replies_to<";
  result += join(input, input + input_size, glue);
  result += ">::with<";
  result += join(output_opt1, output_opt1 + output_opt1_size, glue);
  result += ">";
  return result;
}

} // namespace caf
