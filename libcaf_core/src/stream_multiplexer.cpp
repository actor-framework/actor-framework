/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/detail/stream_multiplexer.hpp"

#include "caf/send.hpp"
#include "caf/variant.hpp"
#include "caf/to_string.hpp"
#include "caf/local_actor.hpp"

namespace caf {
namespace detail {

stream_multiplexer::backend::backend(actor basp_ref) : basp_(basp_ref) {
  // nop
}

stream_multiplexer::backend::~backend() {
  // nop
}

void stream_multiplexer::backend::add_credit(const node_id& nid, int32_t x) {
  auto i = remotes().find(nid);
  if (i != remotes().end()) {
    auto& path = i->second;
    path.credit += x;
    drain_buf(path);
  }
}

void stream_multiplexer::backend::drain_buf(remote_path& path) {
  CAF_LOG_TRACE(CAF_ARG(path));
  auto n = std::min(path.credit, static_cast<int32_t>(path.buf.size()));
  if (n > 0) {
    auto b = path.buf.begin();
    auto e = b + n;
    for (auto i = b; i != e; ++i)
      basp()->enqueue(std::move(*i), nullptr);
    path.buf.erase(b, e);
    path.credit -= static_cast<int32_t>(n);
  }
}

stream_multiplexer::stream_multiplexer(local_actor* self, backend& service)
    : self_(self),
      service_(service) {
  CAF_ASSERT(self_ != nullptr);
}

optional<stream_multiplexer::remote_path&>
stream_multiplexer::get_remote_or_try_connect(const node_id& nid) {
  auto i = remotes().find(nid);
  if (i != remotes().end())
    return i->second;
  auto res = service_.remote_stream_serv(nid);
  if (res)
    return remotes().emplace(nid, std::move(res)).first->second;
  return none;
}

optional<stream_multiplexer::stream_state&>
stream_multiplexer::state_for(const stream_id& sid) {
  auto i = streams_.find(sid);
  if (i != streams_.end())
    return i->second;
  return none;
}

void stream_multiplexer::manage_credit() {
  auto& path = *current_stream_state_->second.rpath;
  // todo: actual, adaptive credit management
  if (--path.in_flight == 0) {
    int32_t new_remote_credit = 5;
    path.in_flight += new_remote_credit;
    send_remote_ctrl(
      path, make_message(sys_atom::value, ok_atom::value, new_remote_credit));
  }
}

void stream_multiplexer::fail(error reason, strong_actor_ptr predecessor,
                                  strong_actor_ptr successor) {
  CAF_ASSERT(current_stream_msg_ != nullptr);
  if (predecessor)
    unsafe_send_as(self_, predecessor,
                   make<stream_msg::abort>(current_stream_msg_->sid, reason));
  if (successor)
    unsafe_send_as(self_, successor,
                   make<stream_msg::abort>(current_stream_msg_->sid, reason));
  auto rp = self_->make_response_promise();
  rp.deliver(std::move(reason));
}

void stream_multiplexer::fail(error reason) {
  CAF_ASSERT(current_stream_msg_ != nullptr);
  auto i = streams_.find(current_stream_msg_->sid);
  if (i != streams_.end()) {
    fail(std::move(reason), std::move(i->second.prev_stage),
         std::move(i->second.next_stage));
    streams_.erase(i);
  } else {
    fail(std::move(reason), nullptr, nullptr);
  }
}

void stream_multiplexer::send_local(strong_actor_ptr& dest, stream_msg&& x,
                                    std::vector<strong_actor_ptr> stages,
                                    message_id mid) {
  CAF_ASSERT(dest != nullptr);
  dest->enqueue(make_mailbox_element(self_->ctrl(), mid, std::move(stages),
                                     std::move(x)),
                self_->context());
}

} // namespace detail
} // namespace caf

