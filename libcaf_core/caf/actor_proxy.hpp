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
#include <cstdint>

#include "caf/abstract_actor.hpp"
#include "caf/monitorable_actor.hpp"

#include "caf/detail/shared_spinlock.hpp"

namespace caf {

/// Represents an actor running on a remote machine,
/// or different hardware, or in a separate process.
class actor_proxy : public monitorable_actor {
public:
  explicit actor_proxy(actor_config& cfg);

  ~actor_proxy() override;

  /// Invokes cleanup code.
  virtual void kill_proxy(execution_unit* ctx, error reason) = 0;
};

} // namespace caf

