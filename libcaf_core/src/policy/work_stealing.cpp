// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/policy/work_stealing.hpp"

#include "caf/actor_system_config.hpp"
#include "caf/config_value.hpp"
#include "caf/defaults.hpp"
#include "caf/scheduler/abstract_coordinator.hpp"

#define CONFIG(str_name, var_name)                                             \
  get_or(p->config(), "work-stealing." str_name,                               \
         defaults::work_stealing::var_name)

namespace caf::policy {

work_stealing::~work_stealing() {
  // nop
}

work_stealing::worker_data::worker_data(scheduler::abstract_coordinator* p)
  : rengine(std::random_device{}()),
    // no need to worry about wrap-around; if `p->num_workers() < 2`,
    // `uniform` will not be used anyway
    uniform(0, p->num_workers() - 2),
    strategies{
      {{CONFIG("aggressive-poll-attempts", aggressive_poll_attempts), 1,
        CONFIG("aggressive-steal-interval", aggressive_steal_interval),
        timespan{0}},
       {CONFIG("moderate-poll-attempts", moderate_poll_attempts), 1,
        CONFIG("moderate-steal-interval", moderate_steal_interval),
        CONFIG("moderate-sleep-duration", moderate_sleep_duration)},
       {1, 0, CONFIG("relaxed-steal-interval", relaxed_steal_interval),
        CONFIG("relaxed-sleep-duration", relaxed_sleep_duration)}}} {
  // nop
}

work_stealing::worker_data::worker_data(const worker_data& other)
  : rengine(std::random_device{}()),
    uniform(other.uniform),
    strategies(other.strategies) {
  // nop
}

} // namespace caf::policy
