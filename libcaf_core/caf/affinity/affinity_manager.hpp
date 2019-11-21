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

#pragma once

#include <atomic>
#include <set>
#include <string>
#include <vector>

#include "caf/actor_system.hpp"
#include "caf/scheduled_actor.hpp"

namespace caf {
namespace affinity {

class manager : public actor_system::module {
public:
  using core_group = std::set<int>;
  using core_groups = std::vector<core_group>;
  using core_array = std::array<core_groups, actor_system::no_id>;
  using atomic_array = std::array<std::atomic<size_t>, actor_system::no_id>;

  explicit manager(actor_system& sys);

  inline std::string worker_cores() const {
    return worker_cores_;
  }

  inline std::string detached_cores() const {
    return detached_cores_;
  }

  inline std::string blocking_cores() const {
    return blocking_cores_;
  }

  inline std::string other_cores() const {
    return other_cores_;
  }

  void set_affinity(actor_system::thread_type tt);

  void start() override;

  void init(actor_system_config& cfg) override;

  id_t id() const override;

  void* subtype_ptr() override;

protected:
  void stop() override;
  actor_system& system_;

private:
  void set_thread_affinity(int pid, core_group cores);

  std::string worker_cores_;
  std::string detached_cores_;
  std::string blocking_cores_;
  std::string other_cores_;

  core_array cores_;
  atomic_array atomics_;
};

} // namespace affinity
} // namespace caf
