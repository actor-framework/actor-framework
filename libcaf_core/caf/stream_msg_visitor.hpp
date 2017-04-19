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
#include "caf/stream_handler.hpp"

namespace caf {

class stream_msg_visitor {
public:
  using map_type = std::unordered_map<stream_id, intrusive_ptr<stream_handler>>;
  using iterator = typename map_type::iterator;
  using result_type = std::pair<error, iterator>;

  stream_msg_visitor(scheduled_actor* self, stream_id& sid,
                     iterator i, iterator last, behavior* bhvr);

  result_type operator()(stream_msg::open& x);

  result_type operator()(stream_msg::ack_open&);

  result_type operator()(stream_msg::batch&);

  result_type operator()(stream_msg::ack_batch&);

  result_type operator()(stream_msg::close&);

  result_type operator()(stream_msg::abort&);

  result_type operator()(stream_msg::downstream_failed&);

  result_type operator()(stream_msg::upstream_failed&);

private:
  scheduled_actor* self_;
  stream_id& sid_;
  iterator i_;
  iterator e_;
  behavior* bhvr_;
};

} // namespace caf

#endif // CAF_STREAM_MSG_VISITOR_HPP
