// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/actor_cast.hpp"
#include "caf/actor_storage.hpp"
#include "caf/actor_system.hpp"
#include "caf/blocking_actor.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/none.hpp"

namespace caf {

/// A scoped handle to a blocking actor.
class CAF_CORE_EXPORT scoped_actor {
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

  explicit operator bool() const {
    return static_cast<bool>(self_);
  }

  actor_system& home_system() const {
    return *self_->home_system;
  }

  blocking_actor* operator->() const {
    return ptr();
  }

  blocking_actor& operator*() const {
    return *ptr();
  }

  actor_addr address() const {
    return ptr()->address();
  }

  blocking_actor* ptr() const;

private:
  actor_control_block* get() const {
    return self_.get();
  }

  actor_id prev_; // used for logging/debugging purposes only
  strong_actor_ptr self_;
};

/// @relates scoped_actor
CAF_CORE_EXPORT std::string to_string(const scoped_actor& x);

} // namespace caf
