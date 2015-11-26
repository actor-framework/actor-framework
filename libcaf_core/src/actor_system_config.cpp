/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/actor_system_config.hpp"

#include <limits>
#include <thread>

namespace caf {

actor_system_config::actor_system_config() {
  // hard coded defaults
  scheduler_policy = "work-stealing";
  scheduler_max_threads = std::max(std::thread::hardware_concurrency(),
                                   unsigned{4});
  scheduler_max_throughput = std::numeric_limits<size_t>::max();
  scheduler_enable_profiling = false;
  scheduler_profiling_ms_resolution = 100;
  middleman_enable_automatic_connections = false;
}

actor_system_config::actor_system_config(int, char**)
    : actor_system_config() {
  // nop
}

actor_system_config&
actor_system_config::add_actor_factory(std::string name, actor_factory fun) {
  actor_factories_.emplace(std::move(name), std::move(fun));
  return *this;
}

actor_system_config&
actor_system_config::set(const std::string& name, config_value val) {
  // TODO: cover all parameters
  if (name == "middleman.enable-automatic-connections" && get<bool>(&val))
    middleman_enable_automatic_connections = get<bool>(val);
  return *this;
}

} // namespace caf
