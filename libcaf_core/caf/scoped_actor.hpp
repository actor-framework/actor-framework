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

#include "caf/none.hpp"
#include "caf/actor_cast.hpp"
#include "caf/actor_system.hpp"
#include "caf/actor_storage.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/blocking_actor.hpp"
#include "caf/scoped_execution_unit.hpp"

namespace caf {

/// A scoped handle to a blocking actor.
class scoped_actor {
public:
  // allow conversion via actor_cast
  template <class, class, int>
  friend class actor_cast_access;

  using signatures = none_t;

  // tell actor_cast which semantic this type uses
  static constexpr bool has_weak_ptr_semantics = false;

  scoped_actor(actor_system& sys, bool hide = false);

  scoped_actor(const scoped_actor&) = delete;
  scoped_actor& operator=(const scoped_actor&) = delete;

  scoped_actor(scoped_actor&&) = delete;
  scoped_actor& operator=(scoped_actor&&) = delete;

  ~scoped_actor();

  inline explicit operator bool() const {
    return static_cast<bool>(self_);
  }

  inline actor_system& home_system() const {
    return *self_->home_system;
  }

  inline blocking_actor* operator->() const {
    return ptr();
  }

  inline blocking_actor& operator*() const {
    return *ptr();
  }

  inline actor_addr address() const {
    return ptr()->address();
  }

  blocking_actor* ptr() const;

private:

  inline actor_control_block* get() const {
    return self_.get();
  }

  actor_id prev_; // used for logging/debugging purposes only
  scoped_execution_unit context_;
  strong_actor_ptr self_;
};

std::string to_string(const scoped_actor& x);

} // namespace caf

