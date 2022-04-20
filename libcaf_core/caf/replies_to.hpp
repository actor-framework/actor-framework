// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <string>

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"

namespace caf {

/// @private
CAF_CORE_EXPORT std::string
replies_to_type_name(size_t input_size, const std::string* input,
                     size_t output_size, const std::string* output);

template <class... Is>
struct [[deprecated("write 'result<foo>(bar)' instead of "
                    "'replies_to<bar>::with<foo>'")]] replies_to {
  template <class... Os>
  using with = result<Os...>(Is...);
};

template <class... Is>
using reacts_to
  [[deprecated("write 'result<void>(foo)' instead of 'reacts_to<foo>'")]]
  = result<void>(Is...);

} // namespace caf
