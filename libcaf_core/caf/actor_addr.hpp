// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/node_id.hpp"

#include <cstddef>
#include <string>
#include <utility>

namespace caf::detail {

/// Customization point for enabling comparison between actor_addr and `Handle`.
template <class Handle>
struct with_actor_addr_from {
  static constexpr bool specialized = false;
};

template <class Handle>
concept has_actor_addr_from = with_actor_addr_from<Handle>::specialized;

} // namespace caf::detail

namespace caf {

/// Identifies an actor by its ID and the node it lives on. An `actor_addr`
/// neither keeps the actor alive nor allows resolving it back to an actor
/// handle. It is used as a lightweight token, e.g., in @ref down_msg and
/// @ref exit_msg, to identify the source of the message.
class CAF_CORE_EXPORT actor_addr {
public:
  // -- constructors, destructors, and assignment operators --------------------

  constexpr actor_addr() noexcept = default;

  /// Constructs an address that identifies an actor with the given ID and
  /// node.
  actor_addr(actor_id id, node_id node) noexcept : id_(id) {
    // Leave the node_id default-constructed if this is the null address. This
    // canonicalizes the representation and makes sure that there is only one
    // representation for the null address.
    if (id != 0 && node) {
      node_ = std::move(node);
    }
  }

  // -- properties -------------------------------------------------------------

  // TODO: return a reference
  static actor_addr from(const strong_actor_ptr& ptr) noexcept;

  // TODO: return a reference
  static actor_addr from(const weak_actor_ptr& ptr) noexcept;

  /// Returns the ID of the identified actor.
  constexpr actor_id id() const noexcept {
    return id_;
  }

  /// Returns the origin node of the identified actor.
  constexpr const node_id& node() const noexcept {
    return node_;
  }

  /// Exchange content of `*this` and `other`.
  void swap(actor_addr& other) noexcept;

  CAF_DEPRECATED("an actor_addr is always valid")
  explicit operator bool() const noexcept {
    return id_ != 0;
  }

  auto operator<=>(const actor_addr&) const noexcept = default;

  template <class Inspector>
  friend bool inspect(Inspector& f, actor_addr& x) {
    auto canonicalize = [&x] {
      if (x.id_ == 0 && x.node_) {
        x.node_ = node_id{};
      }
      return true;
    };
    return f.object(x)
      .on_load(canonicalize)
      .fields(f.field("id", x.id_), f.field("node", x.node_));
  }

  size_t hash() const noexcept;

private:
  explicit actor_addr(actor_control_block* ptr) noexcept;

  actor_id id_ = 0;
  node_id node_;
};

/// @relates actor_addr
CAF_CORE_EXPORT std::string to_string(const actor_addr& x);

/// @relates actor_addr
CAF_CORE_EXPORT void append_to_string(std::string& dst, const actor_addr& x);

/// @relates actor_addr
inline bool operator==(const actor_addr& lhs,
                       const strong_actor_ptr& rhs) noexcept {
  return lhs == actor_addr::from(rhs);
}

/// @relates actor_addr
inline auto operator<=>(const actor_addr& lhs,
                        const strong_actor_ptr& rhs) noexcept {
  return lhs <=> actor_addr::from(rhs);
}

/// @relates actor_addr
inline bool operator==(const strong_actor_ptr& lhs,
                       const actor_addr& rhs) noexcept {
  return actor_addr::from(lhs) == rhs;
}

/// @relates actor_addr
inline auto operator<=>(const strong_actor_ptr& lhs,
                        const actor_addr& rhs) noexcept {
  return actor_addr::from(lhs) <=> rhs;
}

/// @relates actor_addr
inline bool operator==(const actor_addr& lhs,
                       const weak_actor_ptr& rhs) noexcept {
  return lhs == actor_addr::from(rhs);
}

/// @relates actor_addr
inline auto operator<=>(const actor_addr& lhs,
                        const weak_actor_ptr& rhs) noexcept {
  return lhs <=> actor_addr::from(rhs);
}

/// @relates actor_addr
inline bool operator==(const weak_actor_ptr& lhs,
                       const actor_addr& rhs) noexcept {
  return actor_addr::from(lhs) == rhs;
}

/// @relates actor_addr
inline auto operator<=>(const weak_actor_ptr& lhs,
                        const actor_addr& rhs) noexcept {
  return actor_addr::from(lhs) <=> rhs;
}

/// @relates actor_addr
template <detail::has_actor_addr_from Handle>
auto operator==(const Handle& lhs, const actor_addr& rhs) noexcept {
  using impl = detail::with_actor_addr_from<Handle>;
  return impl::visit(lhs, [&rhs](const auto& addr) { return addr == rhs; });
}

/// @relates actor_addr
template <detail::has_actor_addr_from Handle>
auto operator<=>(const Handle& lhs, const actor_addr& rhs) noexcept {
  using impl = detail::with_actor_addr_from<Handle>;
  return impl::visit(lhs, [&rhs](const auto& addr) { return addr <=> rhs; });
}

/// @relates actor_addr
template <detail::has_actor_addr_from Handle>
auto operator==(const actor_addr& lhs, const Handle& rhs) noexcept {
  using impl = detail::with_actor_addr_from<Handle>;
  return impl::visit(rhs, [&lhs](const auto& addr) { return lhs == addr; });
}

/// @relates actor_addr
template <detail::has_actor_addr_from Handle>
auto operator<=>(const actor_addr& lhs, const Handle& rhs) noexcept {
  using impl = detail::with_actor_addr_from<Handle>;
  return impl::visit(rhs, [&lhs](const auto& addr) { return lhs <=> addr; });
}

} // namespace caf

namespace std {

template <>
struct hash<caf::actor_addr> {
  size_t operator()(const caf::actor_addr& ref) const noexcept {
    return ref.hash();
  }
};

} // namespace std
