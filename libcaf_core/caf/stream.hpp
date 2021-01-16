// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/fwd.hpp"
#include "caf/type_id.hpp"

namespace caf {

/// Empty marker type for streaming handshakes.
template <class T>
class stream {
public:
  using value_type = T;
};

/// @relates stream
template <class Inspector, class T>
auto inspect(Inspector& f, stream<T>& x) {
  return f.object(x).fields();
}

} // namespace caf
