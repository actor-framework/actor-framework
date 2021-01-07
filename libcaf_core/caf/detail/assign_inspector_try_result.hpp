// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/error.hpp"
#include "caf/error_code.hpp"
#include "caf/sec.hpp"
#include "caf/type_id.hpp"

namespace caf::detail {

template <class T>
void assign_inspector_try_result(error& x, T&& y) {
  x = std::forward<T>(y);
}

inline void assign_inspector_try_result(error_code<sec>& x, const error& y) {
  if (y.category() == type_id_v<sec>)
    x = static_cast<sec>(y.code());
  else
    x = sec::runtime_error;
}

inline void assign_inspector_try_result(error_code<sec>& x, error_code<sec> y) {
  x = y;
}

} // namespace caf::detail
