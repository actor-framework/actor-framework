/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
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

#include <atomic>

#include "caf/fwd.hpp"
#include "caf/error.hpp"
#include "caf/node_id.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/weak_intrusive_ptr.hpp"

#include "caf/meta/type_name.hpp"
#include "caf/meta/save_callback.hpp"
#include "caf/meta/load_callback.hpp"
#include "caf/meta/omittable_if_none.hpp"

namespace caf {

/// Actors are always allocated with a control block that stores its identity
/// as well as strong and weak reference counts to it. Unlike
/// "common" weak pointer designs, the goal is not to allocate the data
/// separately. Instead, the only goal is to break cycles. For
/// example, linking two actors automatically creates a cycle when using
/// strong reference counts only.
///
/// When allocating a new actor, CAF will always embed the user-defined
/// actor in an `actor_storage` with the control block prefixing the
/// actual actor type, as shown below.
///
///     +----------------------------------------+
///     |            actor_storage<T>            |
///     +----------------------------------------+
///     | +-----------------+------------------+ |
///     | |  control block  |  actor data (T)  | |
///     | +-----------------+------------------+ |
///     | | ref count       | mailbox          | |
///     | | weak ref count  | .                | |
///     | | actor ID        | .                | |
///     | | node ID         | .                | |
///     | +-----------------+------------------+ |
///     +----------------------------------------+
///
/// Actors start with a strong reference count of 1. This count is transferred
/// to the first `actor` or `typed_actor` handle used to store the actor.
/// Actors will also start with a weak reference count of 1. This count
/// is decremenated once the strong reference count drops to 0.
///
/// The data block is destructed by calling the destructor of `T` when the
/// last strong reference expires. The storage itself is destroyed when
/// the last weak reference expires.
class actor_control_block {
public:
  using data_destructor = void (*)(abstract_actor*);
  using block_destructor = void (*)(actor_control_block*);

  actor_control_block(actor_id x, node_id& y, actor_system* sys,
                      data_destructor ddtor, block_destructor bdtor)
      : strong_refs(1),
        weak_refs(1),
        aid(x),
        nid(std::move(y)),
        home_system(sys),
        data_dtor(ddtor),
        block_dtor(bdtor) {
    // nop
  }

  actor_control_block(const actor_control_block&) = delete;
  actor_control_block& operator=(const actor_control_block&) = delete;

  std::atomic<size_t> strong_refs;
  std::atomic<size_t> weak_refs;
  const actor_id aid;
  const node_id nid;
  actor_system* const home_system;
  const data_destructor data_dtor;
  const block_destructor block_dtor;

  static_assert(sizeof(std::atomic<size_t>) == sizeof(void*),
                "std::atomic not lockfree on this platform");

  static_assert(sizeof(intrusive_ptr<node_id::data>) == sizeof(void*),
                "intrusive_ptr<T> and T* have different size");

  static_assert(sizeof(node_id) == sizeof(void*),
                "sizeof(node_id) != sizeof(size_t)");

  static_assert(sizeof(data_destructor) == sizeof(void*),
                "functiion pointer and regular pointers have different size");

  /// Returns a pointer to the actual actor instance.
  inline abstract_actor* get() {
    // this pointer arithmetic is compile-time checked in actor_storage's ctor
    return reinterpret_cast<abstract_actor*>(
      reinterpret_cast<intptr_t>(this) + CAF_CACHE_LINE_SIZE);
  }

  /// Returns a pointer to the control block that stores
  /// identity and reference counts for this actor.
  static actor_control_block* from(const abstract_actor* ptr) {
    // this pointer arithmetic is compile-time checked in actor_storage's ctor
    return reinterpret_cast<actor_control_block*>(
          reinterpret_cast<intptr_t>(ptr) - CAF_CACHE_LINE_SIZE);
  }

  /// @cond PRIVATE

  actor_addr address();

  inline actor_id id() const noexcept {
    return aid;
  }

  inline const node_id& node() const noexcept {
    return nid;
  }

  void enqueue(strong_actor_ptr sender, message_id mid,
               message content, execution_unit* host);

  void enqueue(mailbox_element_ptr what, execution_unit* host);

  /// @endcond
};

/// @relates actor_control_block
bool intrusive_ptr_upgrade_weak(actor_control_block* x);

/// @relates actor_control_block
inline void intrusive_ptr_add_weak_ref(actor_control_block* x) {
  x->weak_refs.fetch_add(1, std::memory_order_relaxed);
}

/// @relates actor_control_block
void intrusive_ptr_release_weak(actor_control_block* x);

/// @relates actor_control_block
inline void intrusive_ptr_add_ref(actor_control_block* x) {
  x->strong_refs.fetch_add(1, std::memory_order_relaxed);
}

/// @relates actor_control_block
void intrusive_ptr_release(actor_control_block* x);

/// @relates abstract_actor
/// @relates actor_control_block
using strong_actor_ptr = intrusive_ptr<actor_control_block>;

/// @relates strong_actor_ptr
bool operator==(const strong_actor_ptr&, const abstract_actor*);

/// @relates strong_actor_ptr
bool operator==(const abstract_actor*, const strong_actor_ptr&);

/// @relates strong_actor_ptr
inline bool operator!=(const strong_actor_ptr& x, const abstract_actor* y) {
  return !(x == y);
}

/// @relates strong_actor_ptr
inline bool operator!=(const abstract_actor* x, const strong_actor_ptr& y) {
  return !(x == y);
}

/// @relates abstract_actor
/// @relates actor_control_block
using weak_actor_ptr = weak_intrusive_ptr<actor_control_block>;

error load_actor(strong_actor_ptr& storage, execution_unit*,
                 actor_id aid, const node_id& nid);

error save_actor(strong_actor_ptr& storage, execution_unit*,
                 actor_id aid, const node_id& nid);

template <class Inspector>
auto context_of(Inspector* f) -> decltype(f->context()) {
  return f->context();
}

inline execution_unit* context_of(void*) {
  return nullptr;
}

std::string to_string(const strong_actor_ptr& x);

void append_to_string(std::string& x, const strong_actor_ptr& y);

std::string to_string(const weak_actor_ptr& x);

void append_to_string(std::string& x, const weak_actor_ptr& y);

template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, strong_actor_ptr& x) {
  actor_id aid = 0;
  node_id nid;
  if (x) {
    aid = x->aid;
    nid = x->nid;
  }
  auto load = [&] { return load_actor(x, context_of(&f), aid, nid); };
  auto save = [&] { return save_actor(x, context_of(&f), aid, nid); };
  return f(meta::type_name("actor"), aid,
           meta::omittable_if_none(), nid,
           meta::load_callback(load), meta::save_callback(save));
}

template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, weak_actor_ptr& x) {
  // inspect as strong pointer, then write back to weak pointer on save
  auto tmp = x.lock();
  auto load = [&]() -> error { x.reset(tmp.get()); return none; };
  return f(tmp, meta::load_callback(load));
}

} // namespace caf

// allow actor pointers to be used in hash maps
namespace std {

template <>
struct hash<caf::strong_actor_ptr> {
  inline size_t operator()(const caf::strong_actor_ptr& ptr) const {
    return ptr ? static_cast<size_t>(ptr->id()) : 0;
  }
};

template <>
struct hash<caf::weak_actor_ptr> {
  inline size_t operator()(const caf::weak_actor_ptr& ptr) const {
    return ptr ? static_cast<size_t>(ptr->id()) : 0;
  }
};

} // namespace std

