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

#include <string>
#include <memory>

#include "caf/fwd.hpp"
#include "caf/actor_addr.hpp"
#include "caf/attachable.hpp"
#include "caf/ref_counted.hpp"
#include "caf/abstract_channel.hpp"

namespace caf {

/// Interface for user-defined multicast implementations.
class group_module {
public:
  // -- constructors, destructors, and assignment operators --------------------

  group_module(actor_system& sys, std::string mname);

  virtual ~group_module();

  // -- pure virtual member functions ------------------------------------------

  /// Stops all groups from this module.
  virtual void stop() = 0;

  /// Returns a pointer to the group associated with the name `group_name`.
  /// @threadsafe
  virtual expected<group> get(const std::string& group_name) = 0;

  /// Loads a group of this module from `source` and stores it in `storage`.
  virtual error load(deserializer& source, group& storage) = 0;

  // -- observers --------------------------------------------------------------

  /// Returns the hosting actor system.
  inline actor_system& system() const {
    return system_;
  }

  /// Returns the name of this module implementation.
  inline const std::string& name() const {
    return name_;
  }

private:
  actor_system& system_;
  std::string name_;
};

} // namespace caf

