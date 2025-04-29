// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/delegated.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/intrusive/stack.hpp"
#include "caf/mailbox_element.hpp"

#include <cstddef>

namespace caf {

/// A simple cache for storing mailbox elements for an actor for later reuse.
class CAF_CORE_EXPORT mail_cache {
public:
  mail_cache(local_actor* self, size_t max_size)
    : self_(self), max_size_(max_size) {
    // nop
  }

  ~mail_cache();

  // -- properties -------------------------------------------------------------

  /// Returns the maximum number of elements this cache can store.
  size_t max_size() const noexcept {
    return max_size_;
  }

  /// Returns the current number of elements in the cache.
  size_t size() const noexcept {
    return size_;
  }

  /// Checks whether the cache is empty.
  bool empty() const noexcept {
    return size_ == 0;
  }

  /// Checks whether the cache reached its maximum size.
  bool full() const noexcept {
    return size_ >= max_size_;
  }

  // -- modifiers --------------------------------------------------------------

  /// Adds `msg` to the cache.
  void stash(message msg);

  /// Adds the current message to the cache. Returns a `delegated<Ts...>` if
  /// `Ts...` is not empty, otherwise returns a `delegated<message>`.
  /// This is a convenience function for `stash(self->current_message())` that
  /// also returns a `delegated` for returning from the current message handler
  /// (can be safely ignored if not needed).
  template <class... Ts>
  auto stash() {
    do_stash_current();
    if constexpr (sizeof...(Ts) == 0) {
      return delegated<message>{};
    } else {
      return delegated<Ts...>{};
    }
  }

  /// Removes all elements from the cache and returns them to the mailbox.
  void unstash();

private:
  void do_stash(mailbox_element* src, message&& msg);

  void do_stash_current();

  local_actor* self_;
  size_t max_size_ = 0;
  size_t size_ = 0;
  intrusive::stack<mailbox_element> elements_;
};

} // namespace caf
