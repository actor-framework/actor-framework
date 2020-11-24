/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2020 Dominik Charousset                                     *
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

#include "caf/actor_traits.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/detail/type_list.hpp"
#include "caf/extend.hpp"
#include "caf/fwd.hpp"
#include "caf/mixin/requester.hpp"
#include "caf/mixin/sender.hpp"
#include "caf/net/abstract_actor_shell.hpp"
#include "caf/net/fwd.hpp"
#include "caf/none.hpp"
#include "caf/typed_actor.hpp"

namespace caf::net {

/// Enables socket managers to communicate with actors using statically typed
/// messaging.
template <class... Sigs>
class typed_actor_shell
  // clang-format off
  : public extend<abstract_actor_shell, typed_actor_shell<Sigs...>>::template
           with<mixin::sender, mixin::requester>,
    public statically_typed_actor_base {
  // clang-format on
public:
  // -- friends ----------------------------------------------------------------

  template <class...>
  friend class typed_actor_shell_ptr;

  // -- member types -----------------------------------------------------------

  // clang-format off
  using super =
    typename extend<abstract_actor_shell, typed_actor_shell<Sigs...>>::template
             with<mixin::sender, mixin::requester>;
  // clang-format on

  using signatures = detail::type_list<Sigs...>;

  using behavior_type = typed_behavior<Sigs...>;

  // -- constructors, destructors, and assignment operators --------------------

  using super::super;

  // -- state modifiers --------------------------------------------------------

  /// Overrides the callbacks for incoming messages.
  template <class... Fs>
  void set_behavior(Fs... fs) {
    auto new_bhvr = behavior_type{std::move(fs)...};
    this->bhvr_ = std::move(new_bhvr.unbox());
  }

  // -- overridden functions of local_actor ------------------------------------

  const char* name() const override {
    return "caf.net.typed-actor-shell";
  }
};

/// An "owning" pointer to an actor shell in the sense that it calls `quit()` on
/// the shell when going out of scope.
template <class... Sigs>
class typed_actor_shell_ptr {
public:
  // -- friends ----------------------------------------------------------------

  friend class socket_manager;

  // -- member types -----------------------------------------------------------

  using handle_type = typed_actor<Sigs...>;

  using element_type = typed_actor_shell<Sigs...>;

  // -- constructors, destructors, and assignment operators --------------------

  constexpr typed_actor_shell_ptr() noexcept {
    // nop
  }

  constexpr typed_actor_shell_ptr(std::nullptr_t) noexcept {
    // nop
  }

  typed_actor_shell_ptr(typed_actor_shell_ptr&& other) noexcept = default;

  typed_actor_shell_ptr&
  operator=(typed_actor_shell_ptr&& other) noexcept = default;

  typed_actor_shell_ptr(const typed_actor_shell_ptr& other) = delete;

  typed_actor_shell_ptr& operator=(const typed_actor_shell_ptr& other) = delete;

  ~typed_actor_shell_ptr() {
    if (auto ptr = get())
      ptr->quit(exit_reason::normal);
  }

  // -- smart pointer interface ------------------------------------------------

  /// Returns an actor handle to the managed actor shell.
  handle_type as_actor() const noexcept {
    return actor_cast<handle_type>(ptr_);
  }

  void detach(error reason) {
    if (auto ptr = get()) {
      ptr->quit(std::move(reason));
      ptr_.release();
    }
  }

  element_type* get() const noexcept {
    if (ptr_) {
      auto ptr = actor_cast<abstract_actor*>(ptr_);
      return static_cast<element_type*>(ptr);
    } else {
      return nullptr;
    }
  }

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
  explicit typed_actor_shell_ptr(strong_actor_ptr ptr) noexcept
    : ptr_(std::move(ptr)) {
    // nop
  }

  strong_actor_ptr ptr_;
};

} // namespace caf::net

namespace caf::detail {

template <class T>
struct typed_actor_shell_ptr_oracle;

template <class... Sigs>
struct typed_actor_shell_ptr_oracle<typed_actor<Sigs...>> {
  using type = net::typed_actor_shell_ptr<Sigs...>;
};

} // namespace caf::detail

namespace caf::net {

template <class Handle>
using typed_actor_shell_ptr_t =
  typename detail::typed_actor_shell_ptr_oracle<Handle>::type;

} // namespace caf::net
