/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENCE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/config.hpp"

#include <map>
#include <mutex>
#include <atomic>
#include <stdexcept>

#include "caf/atom.hpp"
#include "caf/config.hpp"
#include "caf/message.hpp"
#include "caf/actor_addr.hpp"
#include "caf/actor_cast.hpp"
#include "caf/abstract_actor.hpp"
#include "caf/system_messages.hpp"

#include "caf/detail/logging.hpp"
#include "caf/detail/singletons.hpp"
#include "caf/detail/actor_registry.hpp"
#include "caf/detail/shared_spinlock.hpp"

namespace caf {

namespace {
using guard_type = std::unique_lock<std::mutex>;
}

// m_exit_reason is guaranteed to be set to 0, i.e., exit_reason::not_exited,
// by std::atomic<> constructor

abstract_actor::abstract_actor(actor_id aid, node_id nid)
    : super(std::move(nid)),
      m_id(aid),
      m_exit_reason(exit_reason::not_exited),
      m_host(nullptr),
      m_flags(0) {
  // nop
}

abstract_actor::abstract_actor()
    : super(detail::singletons::get_node_id()),
      m_id(detail::singletons::get_actor_registry()->next_id()),
      m_exit_reason(exit_reason::not_exited),
      m_host(nullptr),
      m_flags(0) {
  // nop
}

bool abstract_actor::link_to_impl(const actor_addr& other) {
  if (other && other != this) {
    guard_type guard{m_mtx};
    auto ptr = actor_cast<abstract_actor_ptr>(other);
    // send exit message if already exited
    if (exited()) {
      ptr->enqueue(address(), message_id::invalid,
                   make_message(exit_msg{address(), exit_reason()}), m_host);
    }
    // add link if not already linked to other
    // (checked by establish_backlink)
    else if (ptr->establish_backlink(address())) {
      m_links.push_back(ptr);
      return true;
    }
  }
  return false;
}

bool abstract_actor::attach(attachable_ptr ptr) {
  if (ptr == nullptr) {
    guard_type guard{m_mtx};
    return m_exit_reason == exit_reason::not_exited;
  }
  uint32_t reason;
  { // lifetime scope of guard
    guard_type guard{m_mtx};
    reason = m_exit_reason;
    if (reason == exit_reason::not_exited) {
      m_attachables.push_back(std::move(ptr));
      return true;
    }
  }
  ptr->actor_exited(reason);
  return false;
}

void abstract_actor::detach(const attachable::token& what) {
  attachable_ptr ptr;
  { // lifetime scope of guard
    guard_type guard{m_mtx};
    auto end = m_attachables.end();
    auto i = std::find_if(m_attachables.begin(), end,
                          [&](attachable_ptr& p) { return p->matches(what); });
    if (i != end) {
      ptr = std::move(*i);
      m_attachables.erase(i);
    }
  }
  // ptr will be destroyed here, without locked mutex
}

void abstract_actor::link_to(const actor_addr& other) {
  static_cast<void>(link_to_impl(other));
}

void abstract_actor::unlink_from(const actor_addr& other) {
  static_cast<void>(unlink_from_impl(other));
}

bool abstract_actor::remove_backlink(const actor_addr& other) {
  if (other && other != this) {
    guard_type guard{m_mtx};
    auto i = std::find(m_links.begin(), m_links.end(), other);
    if (i != m_links.end()) {
      m_links.erase(i);
      return true;
    }
  }
  return false;
}

bool abstract_actor::establish_backlink(const actor_addr& other) {
  uint32_t reason = exit_reason::not_exited;
  if (other && other != this) {
    guard_type guard{m_mtx};
    reason = m_exit_reason;
    if (reason == exit_reason::not_exited) {
      auto i = std::find(m_links.begin(), m_links.end(), other);
      if (i == m_links.end()) {
        m_links.push_back(actor_cast<abstract_actor_ptr>(other));
        return true;
      }
    }
  }
  // send exit message without lock
  if (reason != exit_reason::not_exited) {
    auto ptr = actor_cast<abstract_actor_ptr>(other);
    ptr->enqueue(address(), message_id::invalid,
                 make_message(exit_msg{address(), exit_reason()}), m_host);
  }
  return false;
}

bool abstract_actor::unlink_from_impl(const actor_addr& other) {
  if (!other) {
    return false;
  }
  guard_type guard{m_mtx};
  // remove_backlink returns true if this actor is linked to other
  auto ptr = actor_cast<abstract_actor_ptr>(other);
  if (!exited() && ptr->remove_backlink(address())) {
    auto i = std::find(m_links.begin(), m_links.end(), ptr);
    CAF_REQUIRE(i != m_links.end());
    m_links.erase(i);
    return true;
  }
  return false;
}

actor_addr abstract_actor::address() const {
  return actor_addr{const_cast<abstract_actor*>(this)};
}

void abstract_actor::cleanup(uint32_t reason) {
  // log as 'actor'
  CAF_LOGM_TRACE("caf::actor", CAF_ARG(m_id) << ", " << CAF_ARG(reason));
  CAF_REQUIRE(reason != exit_reason::not_exited);
  // move everyhting out of the critical section before processing it
  decltype(m_links) mlinks;
  decltype(m_attachables) mattachables;
  { // lifetime scope of guard
    guard_type guard{m_mtx};
    if (m_exit_reason != exit_reason::not_exited) {
      // already exited
      return;
    }
    m_exit_reason = reason;
    mlinks = std::move(m_links);
    mattachables = std::move(m_attachables);
    // make sure lists are empty
    m_links.clear();
    m_attachables.clear();
  }
  CAF_LOG_INFO_IF(!is_remote(), "actor with ID "
                                << m_id << " had " << mlinks.size()
                                << " links and " << mattachables.size()
                                << " attached functors; exit reason = "
                                << reason << ", class = "
                                << detail::demangle(typeid(*this)));
  // send exit messages
  auto msg = make_message(exit_msg{address(), reason});
  CAF_LOGM_DEBUG("caf::actor", "send EXIT to " << mlinks.size() << " links");
  for (auto& aptr : mlinks) {
    aptr->enqueue(address(), message_id {}.with_high_priority(), msg, m_host);
  }
  CAF_LOGM_DEBUG("caf::actor", "run " << mattachables.size() << " attachables");
  for (attachable_ptr& ptr : mattachables) {
    ptr->actor_exited(reason);
  }
}

std::set<std::string> abstract_actor::message_types() const {
  // defaults to untyped
  return std::set<std::string>{};
}

} // namespace caf
