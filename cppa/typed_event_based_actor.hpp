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


#ifndef CPPA_TYPED_EVENT_BASED_ACTOR_HPP
#define CPPA_TYPED_EVENT_BASED_ACTOR_HPP

#include "cppa/replies_to.hpp"
#include "cppa/local_actor.hpp"
#include "cppa/sync_sender.hpp"
#include "cppa/mailbox_based.hpp"
#include "cppa/typed_behavior.hpp"
#include "cppa/behavior_stack_based.hpp"

namespace cppa {

/**
 * @brief A cooperatively scheduled, event-based actor implementation
 *        with strong type checking.
 *
 * This is the recommended base class for user-defined actors and is used
 * implicitly when spawning typed, functor-based actors without the
 * {@link blocking_api} flag.
 *
 * @extends local_actor
 */
template<typename... Rs>
class typed_event_based_actor
        : public extend<local_actor, typed_event_based_actor<Rs...>>::template
                 with<mailbox_based,
                      behavior_stack_based<
                          typed_behavior<Rs...>
                      >::template impl,
                      sync_sender<
                          nonblocking_response_handle_tag
                      >::template impl> {

 public:

    typed_event_based_actor() : m_initialized(false) { }

    typedef util::type_list<Rs...> signatures;

    typedef typed_behavior<Rs...> behavior_type;

    std::set<std::string> interface() const override {
        return {detail::to_uniform_name<Rs>()...};
    }

 protected:

    virtual behavior_type make_behavior() = 0;

    bool m_initialized;

};

} // namespace cppa

#endif // CPPA_TYPED_EVENT_BASED_ACTOR_HPP
