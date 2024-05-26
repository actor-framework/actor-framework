// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/parser_state.hpp"

#include "caf/detail/format.hpp"
#include "caf/error.hpp"

namespace caf {

error parser_state_to_error(pec code, int32_t line, int32_t column) {
  return {code, detail::format("error in line {} column {}: {}", line, column,
                               to_string(code))};
}

} // namespace caf
