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

#ifndef CAF_GROUP_MANAGER_HPP
#define CAF_GROUP_MANAGER_HPP

#include <map>
#include <mutex>
#include <thread>

#include "caf/fwd.hpp"
#include "caf/expected.hpp"
#include "caf/abstract_group.hpp"
#include "caf/detail/shared_spinlock.hpp"

#include "caf/detail/singleton_mixin.hpp"

namespace caf {

class group_manager {
public:
  friend class actor_system;

  void start();

  void stop();


  ~group_manager();

  /// Get a pointer to the group associated with
  /// `identifier` from the module `mod_name`.
  /// @threadsafe
  expected<group> get(const std::string& module_name,
                      const std::string& group_identifier);

  /// Get a pointer to the group associated with
  /// `identifier` from the module `local`.
  /// @threadsafe
  group get_local(const std::string& group_identifier);

  /// Returns an anonymous group.
  /// Each calls to this member function returns a new instance
  /// of an anonymous group. Anonymous groups can be used whenever
  /// a set of actors wants to communicate using an exclusive channel.
  group anonymous();

  void add_module(abstract_group::unique_module_ptr);

  abstract_group::module_ptr get_module(const std::string& module_name);

private:
  using modules_map = std::map<std::string, abstract_group::unique_module_ptr>;

  group_manager(actor_system& sys);

  modules_map mmap_;
  std::mutex mmap_mtx_;
  actor_system& system_;
};

} // namespace caf

#endif // CAF_GROUP_MANAGER_HPP
