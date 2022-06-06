// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/actor_traits.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/extend.hpp"
#include "caf/fwd.hpp"
#include "caf/mixin/requester.hpp"
#include "caf/mixin/sender.hpp"
#include "caf/net/abstract_actor_shell.hpp"
#include "caf/net/fwd.hpp"
#include "caf/none.hpp"

namespace caf::net {

/// Enables socket managers to communicate with actors using dynamically typed
/// messaging.
class CAF_NET_EXPORT actor_shell
  : public extend<abstract_actor_shell, actor_shell>::with<mixin::sender,
                                                           mixin::requester>,
    public dynamically_typed_actor_base {
public:
  // -- friends ----------------------------------------------------------------

  friend class actor_shell_ptr;

  // -- member types -----------------------------------------------------------

  using super = extend<abstract_actor_shell,
                       actor_shell>::with<mixin::sender, mixin::requester>;

  using signatures = none_t;

  using behavior_type = behavior;

  // -- constructors, destructors, and assignment operators --------------------

  actor_shell(actor_config& cfg, async::execution_context_ptr loop);

  ~actor_shell() override;

  // -- state modifiers --------------------------------------------------------

  /// Overrides the callbacks for incoming messages.
  template <class... Fs>
  void set_behavior(Fs... fs) {
    set_behavior_impl(behavior{std::move(fs)...});
  }

  // -- overridden functions of local_actor ------------------------------------

  const char* name() const override;
};

/// An "owning" pointer to an actor shell in the sense that it calls `quit()` on
/// the shell when going out of scope.
class CAF_NET_EXPORT actor_shell_ptr {
public:
  // -- friends ----------------------------------------------------------------

  template <class Handle>
  friend actor_shell_ptr_t<Handle>
  make_actor_shell(actor_system&, async::execution_context_ptr);

  // -- member types -----------------------------------------------------------

  using handle_type = actor;

  using element_type = actor_shell;

  // -- constructors, destructors, and assignment operators --------------------

  constexpr actor_shell_ptr() noexcept {
    // nop
  }

  constexpr actor_shell_ptr(std::nullptr_t) noexcept {
    // nop
  }

  actor_shell_ptr(actor_shell_ptr&& other) noexcept = default;

  actor_shell_ptr& operator=(actor_shell_ptr&& other) noexcept = default;

  actor_shell_ptr(const actor_shell_ptr& other) = delete;

  actor_shell_ptr& operator=(const actor_shell_ptr& other) = delete;

  ~actor_shell_ptr();

  // -- smart pointer interface ------------------------------------------------

  /// Returns an actor handle to the managed actor shell.
  handle_type as_actor() const noexcept;

  void detach(error reason);

  element_type* get() const noexcept;

  element_type* operator->() const noexcept {
    return get();
  }

  element_type& operator*() const noexcept {
    return *get();
  }

  bool operator!() const noexcept {
    return !ptr_;
  }

  explicit operator bool() const noexcept {
    return static_cast<bool>(ptr_);
  }

private:
  /// @pre `ptr != nullptr`
  explicit actor_shell_ptr(strong_actor_ptr ptr) noexcept;

  strong_actor_ptr ptr_;
};

} // namespace caf::net
