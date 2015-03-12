/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
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

#ifndef CAF_DETAIL_SYNC_REQUEST_BOUNCER_HPP
#define CAF_DETAIL_SYNC_REQUEST_BOUNCER_HPP

#include <cstdint>

namespace caf {

class actor_addr;
class message_id;
class local_actor;
class mailbox_element;

} // namespace caf

namespace caf {
namespace detail {

struct sync_request_bouncer {
  uint32_t rsn;
  explicit sync_request_bouncer(uint32_t r);
  void operator()(const actor_addr& sender, const message_id& mid) const;
  void operator()(const mailbox_element& e) const;

};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_SYNC_REQUEST_BOUNCER_HPP
