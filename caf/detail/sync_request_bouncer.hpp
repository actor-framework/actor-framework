/******************************************************************************\
 *           ___        __                                                    *
 *          /\_ \    __/\ \                                                   *
 *          \//\ \  /\_\ \ \____    ___   _____   _____      __               *
 *            \ \ \ \/\ \ \ '__`\  /'___\/\ '__`\/\ '__`\  /'__`\             *
 *             \_\ \_\ \ \ \ \L\ \/\ \__/\ \ \L\ \ \ \L\ \/\ \L\.\_           *
 *             /\____\\ \_\ \_,__/\ \____\\ \ ,__/\ \ ,__/\ \__/.\_\          *
 *             \/____/ \/_/\/___/  \/____/ \ \ \/  \ \ \/  \/__/\/_/          *
 *                                          \ \_\   \ \_\                     *
 *                                           \/_/    \/_/                     *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the Boost Software License, Version 1.0. See             *
 * accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt  *
\******************************************************************************/

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
