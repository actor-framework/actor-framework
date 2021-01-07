// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/io_export.hpp"
#include "caf/fwd.hpp"

namespace caf::io {

struct connection_helper_state {
  static inline const char* name = "caf.system.connection-helper";
};

CAF_IO_EXPORT behavior
connection_helper(stateful_actor<connection_helper_state>* self, actor b);

} // namespace caf::io
