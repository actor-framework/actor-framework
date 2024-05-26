// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/abstract_actor.hpp"
#include "caf/actor_control_block.hpp"
#include "caf/detail/comparable.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <type_traits>

namespace caf {

/// Stores the address of typed as well as untyped actors.
class CAF_CORE_EXPORT actor_addr
  : detail::comparable<actor_addr>,
    detail::comparable<actor_addr, weak_actor_ptr>,
    detail::comparable<actor_addr, strong_actor_ptr>,
    detail::comparable<actor_addr, abstract_actor*>,
    detail::comparable<actor_addr, actor_control_block*> {
public:
  // -- friend types that need access to private ctors

  friend class abstract_actor;

  // allow conversion via actor_cast
  template <class, class, int>
  friend class actor_cast_access;

  // tell actor_cast which semantic this type uses
  static constexpr bool has_weak_ptr_semantics = true;

  actor_addr() = default;
  actor_addr(actor_addr&&) = default;
  actor_addr(const actor_addr&) = default;
  actor_addr& operator=(actor_addr&&) = default;
  actor_addr& operator=(const actor_addr&) = default;

  actor_addr(std::nullptr_t);

  actor_addr& operator=(std::nullptr_t);

  /// Returns the ID of this actor.
  actor_id id() const noexcept {
    return ptr_->id();
  }

  /// Returns the origin node of this actor.
  node_id node() const noexcept {
    return ptr_->node();
  }

  /// Returns the hosting actor system.
  actor_system& home_system() const noexcept {
    return *ptr_->home_system;
  }

  /// Exchange content of `*this` and `other`.
  void swap(actor_addr& other) noexcept;

  explicit operator bool() const {
    return static_cast<bool>(ptr_);
  }

  /// @cond PRIVATE

  static intptr_t compare(const actor_control_block* lhs,
                          const actor_control_block* rhs);

  intptr_t compare(const actor_addr& other) const noexcept;

  intptr_t compare(const abstract_actor* other) const noexcept;

  intptr_t compare(const actor_control_block* other) const noexcept;

  intptr_t compare(const weak_actor_ptr& other) const noexcept {
    return compare(other.get());
  }

  intptr_t compare(const strong_actor_ptr& other) const noexcept {
    return compare(other.get());
  }

  friend std::string to_string(const actor_addr& x) {
    return to_string(x.ptr_);
  }

  friend void append_to_string(std::string& x, const actor_addr& y) {
    return append_to_string(x, y.ptr_);
  }

  template <class Inspector>
  friend bool inspect(Inspector& f, actor_addr& x) {
    return inspect(f, x.ptr_);
  }

  /// Releases the reference held by handle `x`. Using the
  /// handle after invalidating it is undefined behavior.
  friend void destroy(actor_addr& x) {
    x.ptr_.reset();
  }

  actor_addr(actor_control_block*, bool);

  actor_control_block* get() const noexcept {
    return ptr_.get();
  }

  /// @endcond

private:
  actor_control_block* release() noexcept {
    return ptr_.release();
  }

  actor_control_block* get_locked() const noexcept {
    return ptr_.get_locked();
  }

  actor_addr(actor_control_block*);

  weak_actor_ptr ptr_;
};

inline bool operator==(const actor_addr& x, std::nullptr_t) {
  return x.get() == nullptr;
}

inline bool operator==(std::nullptr_t, const actor_addr& x) {
  return x.get() == nullptr;
}

inline bool operator!=(const actor_addr& x, std::nullptr_t) {
  return x.get() != nullptr;
}

inline bool operator!=(std::nullptr_t, const actor_addr& x) {
  return x.get() != nullptr;
}

} // namespace caf

// allow actor_addr to be used in hash maps
namespace std {
template <>
struct hash<caf::actor_addr> {
  size_t operator()(const caf::actor_addr& ref) const {
    return static_cast<size_t>(ref.id());
  }
};
} // namespace std
