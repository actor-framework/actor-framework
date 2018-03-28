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

/// A multicast group.
class abstract_group : public ref_counted, public abstract_channel {
public:
  // -- member types -----------------------------------------------------------

  friend class local_actor;
  friend class subscription;
  friend class detail::group_manager;

  // -- constructors, destructors, and assignment operators --------------------

  ~abstract_group() override;

  // -- pure virtual member functions ------------------------------------------

  /// Serialize this group to `sink`.
  virtual error save(serializer& sink) const = 0;

  /// Subscribes `who` to this group and returns `true` on success
  /// or `false` if `who` is already subscribed.
  virtual bool subscribe(strong_actor_ptr who) = 0;

  /// Unsubscribes `who` from this group.
  virtual void unsubscribe(const actor_control_block* who) = 0;

  /// Stops any background actors or threads and IO handles.
  virtual void stop() = 0;

  // -- observers --------------------------------------------------------------

  /// Returns the parent module.
  inline group_module& module() const {
    return parent_;
  }

  /// Returns the hosting system.
  inline actor_system& system() const {
    return system_;
  }

  /// Returns a string representation of the group identifier, e.g.,
  /// "224.0.0.1" for IPv4 multicast or a user-defined string for local groups.
  const std::string& identifier() const {
    return identifier_;
  }

protected:
  abstract_group(group_module& mod, std::string id, node_id nid);

  actor_system& system_;
  group_module& parent_;
  std::string identifier_;
  node_id origin_;
};

/// A smart pointer type that manages instances of {@link group}.
/// @relates group
using abstract_group_ptr = intrusive_ptr<abstract_group>;

} // namespace caf

