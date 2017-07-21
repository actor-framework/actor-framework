/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
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

#ifndef CAF_STREAM_MSG_VISITOR_HPP
#define CAF_STREAM_MSG_VISITOR_HPP

#include <utility>
#include <unordered_map>

#include "caf/fwd.hpp"
#include "caf/stream_msg.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/stream_manager.hpp"
#include "caf/scheduled_actor.hpp"

namespace caf {

class stream_msg_visitor {
public:
  using map_type = std::unordered_map<stream_id, intrusive_ptr<stream_manager>>;
  using iterator = typename map_type::iterator;
  using result_type = bool;

  stream_msg_visitor(scheduled_actor* self, const stream_msg& msg,
                     behavior* bhvr);

  result_type operator()(stream_msg::open& x);

  result_type operator()(stream_msg::ack_open& x);

  result_type operator()(stream_msg::batch& x);

  result_type operator()(stream_msg::ack_batch& x);

  result_type operator()(stream_msg::close& x);

  result_type operator()(stream_msg::drop& x);

  result_type operator()(stream_msg::forced_close& x);

  result_type operator()(stream_msg::forced_drop& x);

private:
  // Invokes `f` on the stream manager. On error calls `abort` on the manager
  // and removes it from the streams map.
  template <class F>
  bool invoke(F f) {
    auto e = self_->streams().end();
    auto i = self_->streams().find(sid_);
    if (i == e)
      return false;
    auto err = f(i->second);
    if (err != none) {
      i->second->abort(std::move(err));
      self_->streams().erase(i);
    } else if (i->second->done()) {
      CAF_LOG_DEBUG("manager reported done, remove from streams");
      i->second->close();
      self_->streams().erase(i);
    }
    return true;
  }

  scheduled_actor* self_;
  const stream_id& sid_;
  const actor_addr& sender_;
  behavior* bhvr_;
};

} // namespace caf

#endif // CAF_STREAM_MSG_VISITOR_HPP
