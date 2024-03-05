// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/abstract_actor_shell.hpp"
#include "caf/net/fwd.hpp"

#include "caf/actor_traits.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/detail/to_statically_typed_trait.hpp"
#include "caf/extend.hpp"
#include "caf/fwd.hpp"
#include "caf/mixin/requester.hpp"
#include "caf/mixin/sender.hpp"
#include "caf/none.hpp"
#include "caf/type_list.hpp"
#include "caf/typed_actor.hpp"

namespace caf::net {

template <class...>
class typed_actor_shell;

/// Enables socket managers to communicate with actors using statically typed
/// messaging.
template <class TraitOrSignature>
class typed_actor_shell<TraitOrSignature>
  // clang-format off
  : public extend<abstract_actor_shell, typed_actor_shell<TraitOrSignature>>::template
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
    typename extend<abstract_actor_shell, typed_actor_shell<TraitOrSignature>>::template
             with<mixin::sender, mixin::requester>;
  // clang-format on

  using trait = detail::to_statically_typed_trait_t<TraitOrSignature>;

  using signatures = typename trait::signatures;

  using behavior_type = typed_behavior<TraitOrSignature>;

  // -- constructors, destructors, and assignment operators --------------------

  using super::super;

  // -- state modifiers --------------------------------------------------------

  /// Overrides the callbacks for incoming messages.
  template <class... Fs>
  void set_behavior(Fs... fs) {
    auto new_bhvr = behavior_type{std::move(fs)...};
    this->set_behavior_impl(std::move(new_bhvr.unbox()));
  }

  // -- overridden functions of local_actor ------------------------------------

  const char* name() const override {
    return "caf.net.typed-actor-shell";
  }
};

/// Enables socket managers to communicate with actors using statically typed
/// messaging.
template <class T1, class T2, class... Ts>
class typed_actor_shell<T1, T2, Ts...>
  // clang-format off
  : public extend<abstract_actor_shell, typed_actor_shell<T1, T2, Ts...>>::template
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
    typename extend<abstract_actor_shell, typed_actor_shell<T1, T2, Ts...>>::template
             with<mixin::sender, mixin::requester>;
  // clang-format on

  using signatures = type_list<T1, T2, Ts...>;

  using behavior_type = typed_behavior<T1, T2, Ts...>;

  // -- constructors, destructors, and assignment operators --------------------

  using super::super;

  // -- state modifiers --------------------------------------------------------

  /// Overrides the callbacks for incoming messages.
  template <class... Fs>
  void set_behavior(Fs... fs) {
    auto new_bhvr = behavior_type{std::move(fs)...};
    this->set_behavior_impl(std::move(new_bhvr.unbox()));
  }

  // -- overridden functions of local_actor ------------------------------------

  const char* name() const override {
    return "caf.net.typed-actor-shell";
  }
};

template <class...>
class typed_actor_shell_ptr;

/// An "owning" pointer to an actor shell in the sense that it calls `quit()` on
/// the shell when going out of scope.
template <class TraitOrSignature>
class typed_actor_shell_ptr<TraitOrSignature> {
public:
  // -- friends ----------------------------------------------------------------

  template <class Handle>
  friend actor_shell_ptr_t<Handle>
  make_actor_shell(actor_system&, async::execution_context_ptr);

  // -- member types -----------------------------------------------------------

  using handle_type = typed_actor<TraitOrSignature>;

  using element_type = typed_actor_shell<TraitOrSignature>;

  // -- constructors, destructors, and assignment operators --------------------

  constexpr typed_actor_shell_ptr() noexcept {
    // nop
  }

  constexpr typed_actor_shell_ptr(std::nullptr_t) noexcept {
    // nop
  }

  typed_actor_shell_ptr(typed_actor_shell_ptr&& other) noexcept = default;

  typed_actor_shell_ptr& operator=(typed_actor_shell_ptr&& other) noexcept
    = default;

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

/// An "owning" pointer to an actor shell in the sense that it calls `quit()` on
/// the shell when going out of scope.
/// @note This is a specialization for backwards compatibility with pre v1.0
///       releases. Please use the trait based implementation.
template <class T1, class T2, class... Ts>
class typed_actor_shell_ptr<T1, T2, Ts...> {
public:
  // -- friends ----------------------------------------------------------------

  template <class Handle>
  friend actor_shell_ptr_t<Handle>
  make_actor_shell(actor_system&, async::execution_context_ptr);

  // -- member types -----------------------------------------------------------

  using handle_type = typed_actor<T1, T2, Ts...>;

  using element_type = typed_actor_shell<T1, T2, Ts...>;

  // -- constructors, destructors, and assignment operators --------------------

  constexpr typed_actor_shell_ptr() noexcept {
    // nop
  }

  constexpr typed_actor_shell_ptr(std::nullptr_t) noexcept {
    // nop
  }

  typed_actor_shell_ptr(typed_actor_shell_ptr&& other) noexcept = default;

  typed_actor_shell_ptr& operator=(typed_actor_shell_ptr&& other) noexcept
    = default;

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
