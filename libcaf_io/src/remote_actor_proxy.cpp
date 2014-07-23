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

#include "caf/send.hpp"
#include "caf/to_string.hpp"

#include "caf/io/middleman.hpp"
#include "caf/io/remote_actor_proxy.hpp"

#include "caf/detail/memory.hpp"
#include "caf/detail/logging.hpp"
#include "caf/detail/singletons.hpp"
#include "caf/detail/sync_request_bouncer.hpp"

using namespace std;

namespace caf {
namespace io {

inline sync_request_info* new_req_info(actor_addr sptr, message_id id) {
  return detail::memory::create<sync_request_info>(std::move(sptr), id);
}

sync_request_info::~sync_request_info() { }

sync_request_info::sync_request_info(actor_addr sptr, message_id id)
    : next(nullptr), sender(std::move(sptr)), mid(id) {
}

remote_actor_proxy::remote_actor_proxy(actor_id aid,
                     node_id nid,
                     actor parent)
    : super(aid, nid), m_parent(parent) {
  CAF_REQUIRE(parent != invalid_actor);
  CAF_LOG_INFO(CAF_ARG(aid) << ", " << CAF_TARG(nid, to_string));
}

remote_actor_proxy::~remote_actor_proxy() {
  anon_send(m_parent, make_message(atom("_DelProxy"),
                   node(),
                   m_id));
}

void remote_actor_proxy::forward_msg(const actor_addr& sender,
                   message_id mid,
                   message msg) {
  CAF_LOG_TRACE(CAF_ARG(m_id)
           << ", " << CAF_TSARG(sender)
           << ", " << CAF_MARG(mid, integer_value)
           << ", " << CAF_TSARG(msg));
  m_parent->enqueue(invalid_actor_addr,
            message_id::invalid,
            make_message(atom("_Dispatch"),
                   sender,
                   address(),
                   mid,
                   std::move(msg)),
            nullptr);
}

void remote_actor_proxy::enqueue(const actor_addr& sender,
                 message_id mid,
                 message m,
                 execution_unit*) {
  forward_msg(sender, mid, std::move(m));
}

void remote_actor_proxy::link_to(const actor_addr& other) {
  if (link_to_impl(other)) {
    // causes remote actor to link to (proxy of) other
    // receiving peer will call: this->local_link_to(other)
    forward_msg(address(), message_id::invalid,
          make_message(atom("_Link"), other));
  }
}

void remote_actor_proxy::unlink_from(const actor_addr& other) {
  if (unlink_from_impl(other)) {
    // causes remote actor to unlink from (proxy of) other
    forward_msg(address(), message_id::invalid,
          make_message(atom("_Unlink"), other));
  }
}

bool remote_actor_proxy::establish_backlink(const actor_addr& other) {
  if (super::establish_backlink(other)) {
    // causes remote actor to unlink from (proxy of) other
    forward_msg(address(), message_id::invalid,
          make_message(atom("_Link"), other));
    return true;
  }
  return false;
}

bool remote_actor_proxy::remove_backlink(const actor_addr& other) {
  if (super::remove_backlink(other)) {
    // causes remote actor to unlink from (proxy of) other
    forward_msg(address(), message_id::invalid,
          make_message(atom("_Unlink"), other));
    return true;
  }
  return false;
}

void remote_actor_proxy::local_link_to(const actor_addr& other) {
  link_to_impl(other);
}

void remote_actor_proxy::local_unlink_from(const actor_addr& other) {
  unlink_from_impl(other);
}

void remote_actor_proxy::kill_proxy(uint32_t reason) {
  cleanup(reason);
}

} // namespace io
} // namespace caf
