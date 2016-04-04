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

#ifndef CAF_SCOPED_ACTOR_HPP
#define CAF_SCOPED_ACTOR_HPP

#include "caf/actor_system.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/blocking_actor.hpp"
#include "caf/scoped_execution_unit.hpp"

namespace caf {

/// A scoped handle to a blocking actor.
class scoped_actor {
public:
  scoped_actor(const scoped_actor&) = delete;

  scoped_actor(actor_system& sys, bool hide_actor = false);

  scoped_actor(scoped_actor&&) = default;
  scoped_actor& operator=(scoped_actor&&) = default;

  ~scoped_actor();

  inline blocking_actor* operator->() const {
    return self_.get();
  }

  inline blocking_actor& operator*() const {
    return *self_;
  }

  inline blocking_actor* get() const {
    return self_.get();
  }

  inline actor_addr address() const {
    return self_->address();
  }

private:
  actor_id prev_; // used for logging/debugging purposes only
  scoped_execution_unit context_;
  intrusive_ptr<blocking_actor> self_;
};

std::string to_string(const scoped_actor& x);

} // namespace caf

#endif // CAF_SCOPED_ACTOR_HPP
