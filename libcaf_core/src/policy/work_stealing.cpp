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

#include "caf/policy/work_stealing.hpp"

#include "caf/actor_system_config.hpp"
#include "caf/config_value.hpp"
#include "caf/defaults.hpp"
#include "caf/scheduler/abstract_coordinator.hpp"

#define CONFIG(str_name, var_name)                                             \
  get_or(p->config(), "work-stealing." str_name,                               \
         defaults::work_stealing::var_name)

namespace caf {
namespace policy {

work_stealing::~work_stealing() {
  // nop
}

work_stealing::worker_data::worker_data(scheduler::abstract_coordinator* p)
    : rengine(std::random_device{}()),
      // no need to worry about wrap-around; if `p->num_workers() < 2`,
      // `uniform` will not be used anyway
      uniform(0, p->num_workers() - 2),
      strategies{{
        {CONFIG("aggressive-poll-attempts", aggressive_poll_attempts), 1,
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

} // namespace policy
} // namespace caf
