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

#include <map>
#include <mutex>
#include <thread>
#include <unordered_map>

#include "caf/fwd.hpp"
#include "caf/optional.hpp"
#include "caf/expected.hpp"
#include "caf/group_module.hpp"
#include "caf/abstract_group.hpp"
#include "caf/detail/shared_spinlock.hpp"

namespace caf {

class group_manager {
public:
  // -- friends ----------------------------------------------------------------

  friend class actor_system;

  // -- member types -----------------------------------------------------------

  using modules_map = std::unordered_map<std::string,
                                         std::unique_ptr<group_module>>;

  // -- constructors, destructors, and assignment operators --------------------

  ~group_manager();

  // -- observers --------------------------------------------------------------

  /// Get a handle to the group associated with given URI scheme.
  /// @threadsafe
  /// @experimental
  expected<group> get(std::string group_uri) const;

  /// Get a handle to the group associated with
  /// `identifier` from the module `mod_name`.
  /// @threadsafe
  expected<group> get(const std::string& module_name,
                      const std::string& group_identifier) const;

  /// Get a pointer to the group associated with
  /// `identifier` from the module `local`.
  /// @threadsafe
  group get_local(const std::string& group_identifier) const;

  /// Returns an anonymous group.
  /// Each calls to this member function returns a new instance
  /// of an anonymous group. Anonymous groups can be used whenever
  /// a set of actors wants to communicate using an exclusive channel.
  group anonymous() const;

  /// Returns the module named `name` if it exists, otherwise `none`.
  optional<group_module&> get_module(const std::string& x) const;

private:
  // -- constructors, destructors, and assignment operators --------------------

  group_manager(actor_system& sys);

  // -- member functions required by actor_system ------------------------------

  void init(actor_system_config& cfg);

  void start();

  void stop();

  // -- data members -----------------------------------------------------------

  modules_map mmap_;
  actor_system& system_;
};

} // namespace caf

