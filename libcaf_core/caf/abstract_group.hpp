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

#include <memory>
#include <string>

#include "caf/abstract_channel.hpp"
#include "caf/actor.hpp"
#include "caf/actor_addr.hpp"
#include "caf/attachable.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/ref_counted.hpp"

namespace caf {

/// A multicast group.
class CAF_CORE_EXPORT abstract_group : public ref_counted,
                                       public abstract_channel {
public:
  // -- member types -----------------------------------------------------------

  friend class local_actor;
  friend class subscription;
  friend class detail::group_manager;

  // -- constructors, destructors, and assignment operators --------------------

  ~abstract_group() override;

  // -- pure virtual member functions ------------------------------------------

  /// Subscribes `who` to this group and returns `true` on success
  /// or `false` if `who` is already subscribed.
  virtual bool subscribe(strong_actor_ptr who) = 0;

  /// Unsubscribes `who` from this group.
  virtual void unsubscribe(const actor_control_block* who) = 0;

  /// Stops any background actors or threads and IO handles.
  virtual void stop() = 0;

  // -- observers --------------------------------------------------------------

  /// Returns the hosting system.
  actor_system& system() const noexcept;

  /// Returns the parent module.
  group_module& module() const noexcept {
    return *parent_;
  }

  /// Returns the origin node of the group if applicable.
  node_id origin() const noexcept {
    return origin_;
  }

  /// Returns a string representation of the group identifier, e.g.,
  /// "224.0.0.1" for IPv4 multicast or a user-defined string for local groups.
  const std::string& identifier() const {
    return identifier_;
  }

  /// Returns a human-readable string representation of the group ID.
  virtual std::string stringify() const;

  /// Returns the intermediary actor for the group if applicable.
  virtual actor intermediary() const noexcept;

protected:
  abstract_group(group_module_ptr mod, std::string id, node_id nid);

  group_module_ptr parent_;
  node_id origin_;
  std::string identifier_;
};

/// A smart pointer type that manages instances of {@link group}.
/// @relates group
using abstract_group_ptr = intrusive_ptr<abstract_group>;

} // namespace caf
