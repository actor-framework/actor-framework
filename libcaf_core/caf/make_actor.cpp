// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/make_actor.hpp"

#include "caf/detail/pretty_type_name.hpp"
#include "caf/logger.hpp"
#include "caf/monitorable_actor.hpp"

#include <typeinfo>

namespace caf::detail {

void CAF_CORE_EXPORT log_spawn_event(monitorable_actor* self) {
  auto* instance = logger::current_logger();
  if (instance == nullptr || instance->accepts(CAF_LOG_LEVEL_DEBUG, "caf_flow"))
    return;
  instance->log_unchecked(CAF_LOG_LEVEL_DEBUG, "caf_flow",
                          "SPAWN ; ID = {} ; NAME = {} ; TYPE = {} ; NODE = {}",
                          self->id(), self->name(),
                          detail::pretty_type_name(typeid(*self)),
                          to_string(self->node()));
}

} // namespace caf::detail
