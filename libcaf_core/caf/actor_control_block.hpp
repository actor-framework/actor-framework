// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/config.hpp"
#include "caf/detail/control_block_ref_count.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/formatted.hpp"
#include "caf/detail/print.hpp"
#include "caf/error_code.hpp"
#include "caf/fwd.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/node_id.hpp"
#include "caf/weak_intrusive_ptr.hpp"

namespace caf {

/// Control block for actor instances. Stores the actor's identity as well as
/// strong and weak reference counts.
///
/// When allocating a new actor, CAF embeds the actor object in the control
/// block to allocate only once. Actors start with a strong reference count of
/// 1. This count is transferred to the first `actor` or `typed_actor` handle
/// used to store the actor. Actors will also start with a weak reference count
/// of 1. This implicit weak reference is released once the strong reference
/// count drops to 0.
///
/// The actor object is destructed when the last strong reference expires. The
/// full memory block is destroyed when the last weak reference expires.
class CAF_CORE_EXPORT actor_control_block {
public:
  friend class detail::control_block_ref_count;

  actor_control_block(actor_id aid, caf::node_id& nid, actor_system* sys,
                      const meta::handler_list* ifptr)
    : aid_(aid), nid_(std::move(nid)), system_(sys), iface_(ifptr) {
    CAF_ASSERT(system_ != nullptr);
  }

  using control_block_type = actor_control_block;

  actor_control_block(const actor_control_block&) = delete;

  actor_control_block& operator=(const actor_control_block&) = delete;

  virtual ~actor_control_block() noexcept;

  /// Returns a pointer to the actor instance.
  abstract_actor* managed() noexcept {
    return managed_;
  }

  CAF_DEPRECATED("use managed() instead")
  abstract_actor* get() noexcept {
    return managed_;
  }

  /// Returns a reference to the actor system that owns this actor.
  actor_system& system() const noexcept {
    return *system_;
  }

  /// Returns an actor address for this actor.
  actor_addr address() noexcept;

  /// Returns the local ID of this actor.
  actor_id id() const noexcept {
    return aid_;
  }

  /// Returns the node ID of this actor.
  const node_id& node() const noexcept {
    return nid_;
  }

  /// Returns the messaging interface of this actor.
  const meta::handler_list* iface() const noexcept {
    return iface_;
  }

  /// Enqueues a message to the mailbox of this actor.
  bool enqueue(mailbox_element_ptr what, scheduler* sched);

  /// Increments the strong reference count of this actor.
  void ref() noexcept {
    ref_count_.inc_strong();
  }

  /// Decrements the strong reference count of this actor.
  void deref() noexcept;

  /// Increments the weak reference count of this actor.
  void ref_weak() noexcept {
    ref_count_.inc_weak();
  }

  /// Decrements the weak reference count of this actor.
  void deref_weak() noexcept {
    ref_count_.dec_weak(this);
  }

  /// Tries to upgrade a weak reference to a strong reference.
  bool upgrade_weak() noexcept {
    return ref_count_.upgrade_weak();
  }

  /// Returns the current number of strong references to this actor.
  size_t strong_reference_count() const noexcept {
    return ref_count_.strong_reference_count();
  }

  /// Returns the current number of weak references to this actor.
  size_t weak_reference_count() const noexcept {
    return ref_count_.weak_reference_count();
  }

  /// Returns a hash value for this actor.
  size_t hash() const noexcept {
    if constexpr (sizeof(size_t) == sizeof(caf::actor_id)) {
      return static_cast<size_t>(aid_);
    } else {
      std::hash<caf::actor_id> hasher;
      return hasher(aid_);
    }
  }

  /// Returns the control block for the given actor instance.
  static actor_control_block* from(const abstract_actor* ptr) noexcept;

protected:
  /// Destroys the control block instance and releases the memory.
  virtual void delete_this() noexcept = 0;

  /// Destroys the managed actor instance.
  void destroy_managed() noexcept;

  /// The intrusive reference count for this control block.
  detail::control_block_ref_count ref_count_;

  /// Stores a pointer to the managed actor instance.
  abstract_actor* managed_;

  /// Stores the actor ID.
  actor_id aid_;

  /// Stores the node ID, i.e., the identifier of the actor's host.
  node_id nid_;

  /// Stores a pointer to the actor system that created this actor.
  actor_system* system_;

  /// Stores a pointer to the messaging interface of this actor.
  const meta::handler_list* iface_;
};

/// @relates actor_control_block
using strong_actor_ptr = intrusive_ptr<actor_control_block>;

/// @relates actor_control_block
using weak_actor_ptr = weak_intrusive_ptr<actor_control_block>;

inline bool operator==(const strong_actor_ptr& lhs,
                       const abstract_actor* rhs) noexcept {
  return lhs.get() == actor_control_block::from(rhs);
}

inline bool operator==(const abstract_actor* lhs,
                       const strong_actor_ptr& rhs) noexcept {
  return actor_control_block::from(lhs) == rhs.get();
}

inline bool operator!=(const strong_actor_ptr& x,
                       const abstract_actor* y) noexcept {
  return !(x == y);
}

inline bool operator!=(const abstract_actor* x,
                       const strong_actor_ptr& y) noexcept {
  return !(x == y);
}

CAF_CORE_EXPORT error_code<sec> load_actor(strong_actor_ptr& ptr,
                                           actor_system* sys, actor_id aid,
                                           const node_id& nid);

CAF_CORE_EXPORT error_code<sec> save_actor(const strong_actor_ptr& ptr,
                                           actor_id aid, const node_id& nid);

CAF_CORE_EXPORT std::string to_string(const strong_actor_ptr& ptr);

CAF_CORE_EXPORT void append_to_string(std::string& str,
                                      const strong_actor_ptr& ptr);

CAF_CORE_EXPORT std::string to_string(const weak_actor_ptr& ptr);

CAF_CORE_EXPORT void append_to_string(std::string& str,
                                      const weak_actor_ptr& ptr);

} // namespace caf

// allow actor pointers to be used in hash maps
namespace std {

template <>
struct hash<caf::strong_actor_ptr> {
  size_t operator()(const caf::strong_actor_ptr& ptr) const noexcept {
    return ptr ? ptr->hash() : 0;
  }
};

template <>
struct hash<caf::weak_actor_ptr> {
  size_t operator()(const caf::weak_actor_ptr& ptr) const noexcept {
    return ptr.hash();
  }
};

} // namespace std

namespace caf::detail {

template <>
struct simple_formatter<actor_control_block> {
  template <class OutputIterator>
  OutputIterator
  format(const actor_control_block& ctrl, OutputIterator out) const {
    using namespace std::literals;
    const simple_formatter<node_id> prefix;
    out = prefix.format(ctrl.node(), out);
    const auto infix = "/actor/id/"sv;
    print_iterator_adapter<OutputIterator> buf{out};
    buf.insert(buf.end(), infix.begin(), infix.end());
    print(buf, ctrl.id());
    return buf.pos;
  }
};

} // namespace caf::detail
