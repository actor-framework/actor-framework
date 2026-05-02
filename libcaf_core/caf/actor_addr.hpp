// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/comparable.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/hash/fnv.hpp"
#include "caf/node_id.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

namespace caf {

/// Identifies an actor by its ID and the node it lives on. An `actor_addr`
/// neither keeps the actor alive nor allows resolving it back to an actor
/// handle. It is used as a lightweight token, e.g., in @ref down_msg and
/// @ref exit_msg, to identify the source of the message.
class CAF_CORE_EXPORT actor_addr
  : detail::comparable<actor_addr>,
    detail::comparable<actor_addr, strong_actor_ptr>,
    detail::comparable<actor_addr, weak_actor_ptr>,
    detail::comparable<actor_addr, abstract_actor*>,
    detail::comparable<actor_addr, actor_control_block*> {
public:
  // -- constructors, destructors, and assignment operators --------------------

  actor_addr() noexcept = default;

  actor_addr(actor_addr&&) noexcept = default;

  actor_addr(const actor_addr&) noexcept = default;

  actor_addr& operator=(actor_addr&&) noexcept = default;

  actor_addr& operator=(const actor_addr&) noexcept = default;

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

  /// Returns the ID of the identified actor.
  actor_id id() const noexcept {
    return id_;
  }

  /// Returns the origin node of the identified actor.
  const node_id& node() const noexcept {
    return node_;
  }

  /// Exchange content of `*this` and `other`.
  void swap(actor_addr& other) noexcept;

  CAF_DEPRECATED("an actor_addr is always valid")
  explicit operator bool() const noexcept {
    return id_ != 0;
  }

  // -- comparison -------------------------------------------------------------

  intptr_t compare(const actor_addr& other) const noexcept;

  intptr_t compare(const strong_actor_ptr& other) const noexcept;

  intptr_t compare(const weak_actor_ptr& other) const noexcept;

  intptr_t compare(const abstract_actor* other) const noexcept;

  intptr_t compare(const actor_control_block* other) const noexcept;

  // -- friend functions -------------------------------------------------------

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

private:
  explicit actor_addr(actor_control_block* ptr) noexcept;

  actor_id id_ = 0;
  node_id node_;
};

/// @relates actor_addr
CAF_CORE_EXPORT std::string to_string(const actor_addr& x);

/// @relates actor_addr
CAF_CORE_EXPORT void append_to_string(std::string& dst, const actor_addr& x);

} // namespace caf

namespace std {

template <>
struct hash<caf::actor_addr> {
  size_t operator()(const caf::actor_addr& ref) const noexcept {
    auto aid = ref.id();
    if (aid == 0) {
      return 0;
    }
    return caf::hash::fnv<size_t>::compute(aid, ref.node());
  }
};

} // namespace std
