// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/config.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/critical.hpp"
#include "caf/fwd.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/node_id.hpp"
#include "caf/weak_intrusive_ptr.hpp"

#include <atomic>

namespace caf {

/// Actors are always allocated with a control block that stores its identity
/// as well as strong and weak reference counts to it. Unlike
/// "common" weak pointer designs, the goal is not to allocate the data
/// separately. Instead, the only goal is to break cycles. For
/// example, linking two actors automatically creates a cycle when using
/// strong reference counts only.
///
/// When allocating a new actor, CAF enforces that the actor object will start
/// exactly `CAF_CACHE_LINE_SIZE` bytes after the start of the control block.
///
///      +-----------------+------------------+
///      |  control block  |  actor data (T)  |
///      +-----------------+------------------+
///      | strong refs     | mailbox          |
///      | weak refs       | ...              |
///      | actor ID        |                  |
///      | node ID         |                  |
///      | ...             |                  |
///      +-----------------+------------------+
///
/// Actors start with a strong reference count of 1. This count is transferred
/// to the first `actor` or `typed_actor` handle used to store the actor.
/// Actors will also start with a weak reference count of 1. This count
/// is decremenated once the strong reference count drops to 0.
///
/// The actor object is when the last strong reference expires. The full memory
/// block is destroyed when the last weak reference expires.
class CAF_CORE_EXPORT actor_control_block {
public:
  actor_control_block(actor_id x, node_id& y, actor_system* sys,
                      const meta::handler_list* iface)
    : strong_refs(1),
      weak_refs(1),
      aid(x),
      nid(std::move(y)),
      home_system(sys),
      iface(iface) {
    // nop
  }

  actor_control_block(const actor_control_block&) = delete;

  actor_control_block& operator=(const actor_control_block&) = delete;

  /// Stores the number of strong references to this actor.
  std::atomic<size_t> strong_refs;

  /// Stores the number of weak references to this actor.
  std::atomic<size_t> weak_refs;

  /// Stores the actor ID.
  const actor_id aid;

  /// Stores the node ID, i.e., the identifier of the actor's host.
  const node_id nid;

  /// Stores a pointer to the actor system that created this actor.
  actor_system* const home_system;

  /// Stores a pointer to the interface of the actor or `nullptr` if the actor
  /// is dynamically typed.
  const meta::handler_list* const iface;

  /// Returns a pointer to the actor instance.
  abstract_actor* get() noexcept {
    // The memory layout is enforced by `make_actor`.
    return reinterpret_cast<abstract_actor*>(reinterpret_cast<intptr_t>(this)
                                             + CAF_CACHE_LINE_SIZE);
  }

  /// Returns a pointer to the control block that stores identity and reference
  /// counts for this actor.
  static actor_control_block* from(const abstract_actor* ptr) noexcept {
    // The memory layout is enforced by `make_actor`.
    return reinterpret_cast<actor_control_block*>(
      reinterpret_cast<intptr_t>(ptr) - CAF_CACHE_LINE_SIZE);
  }

  /// @cond

  actor_addr address() noexcept;

  actor_id id() const noexcept {
    return aid;
  }

  const node_id& node() const noexcept {
    return nid;
  }

  bool enqueue(mailbox_element_ptr what, scheduler* sched);

  /// @endcond
};

static_assert(sizeof(actor_control_block) <= CAF_CACHE_LINE_SIZE,
              "actor_control_block may not exceed a cache line");

/// @relates actor_control_block
CAF_CORE_EXPORT bool intrusive_ptr_upgrade_weak(actor_control_block* x);

/// @relates actor_control_block
inline void intrusive_ptr_add_weak_ref(actor_control_block* x) {
#ifdef NDEBUG
  x->weak_refs.fetch_add(1, std::memory_order_relaxed);
#else
  if (x->weak_refs.fetch_add(1, std::memory_order_relaxed) == 0)
    CAF_CRITICAL("increased the weak reference count of an expired actor");
#endif
}

/// @relates actor_control_block
CAF_CORE_EXPORT void intrusive_ptr_release_weak(actor_control_block* x);

/// @relates actor_control_block
inline void intrusive_ptr_add_ref(actor_control_block* x) {
#ifdef NDEBUG
  x->strong_refs.fetch_add(1, std::memory_order_relaxed);
#else
  if (x->strong_refs.fetch_add(1, std::memory_order_relaxed) == 0)
    CAF_CRITICAL("increased the strong reference count of an expired actor");
#endif
}

/// @relates actor_control_block
CAF_CORE_EXPORT void intrusive_ptr_release(actor_control_block* x);

/// @relates actor_control_block
using strong_actor_ptr = intrusive_ptr<actor_control_block>;

CAF_CORE_EXPORT bool operator==(const strong_actor_ptr&,
                                const abstract_actor*) noexcept;

CAF_CORE_EXPORT bool operator==(const abstract_actor*,
                                const strong_actor_ptr&) noexcept;

inline bool operator!=(const strong_actor_ptr& x,
                       const abstract_actor* y) noexcept {
  return !(x == y);
}

inline bool operator!=(const abstract_actor* x,
                       const strong_actor_ptr& y) noexcept {
  return !(x == y);
}

/// @relates actor_control_block
using weak_actor_ptr = weak_intrusive_ptr<actor_control_block>;

CAF_CORE_EXPORT error_code<sec> load_actor(strong_actor_ptr& storage,
                                           actor_system*, actor_id aid,
                                           const node_id& nid);

CAF_CORE_EXPORT error_code<sec> save_actor(const strong_actor_ptr& ptr,
                                           actor_id aid, const node_id& nid);

CAF_CORE_EXPORT std::string to_string(const strong_actor_ptr& x);

CAF_CORE_EXPORT void append_to_string(std::string& x,
                                      const strong_actor_ptr& y);

CAF_CORE_EXPORT std::string to_string(const weak_actor_ptr& x);

CAF_CORE_EXPORT void append_to_string(std::string& x, const weak_actor_ptr& y);

} // namespace caf

// allow actor pointers to be used in hash maps
namespace std {

template <>
struct hash<caf::strong_actor_ptr> {
  size_t operator()(const caf::strong_actor_ptr& ptr) const noexcept {
    return ptr ? static_cast<size_t>(ptr->id()) : 0;
  }
};

template <>
struct hash<caf::weak_actor_ptr> {
  size_t operator()(const caf::weak_actor_ptr& ptr) const noexcept {
    return ptr ? static_cast<size_t>(ptr->id()) : 0;
  }
};

} // namespace std
