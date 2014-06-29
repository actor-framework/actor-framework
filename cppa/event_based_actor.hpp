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

#ifndef CPPA_UNTYPED_ACTOR_HPP
#define CPPA_UNTYPED_ACTOR_HPP

#include <type_traits>

#include "cppa/on.hpp"
#include "cppa/extend.hpp"
#include "cppa/local_actor.hpp"
#include "cppa/response_handle.hpp"

#include "cppa/mixin/sync_sender.hpp"
#include "cppa/mixin/mailbox_based.hpp"
#include "cppa/mixin/functor_based.hpp"
#include "cppa/mixin/behavior_stack_based.hpp"

#include "cppa/detail/logging.hpp"
#include "cppa/detail/response_handle_util.hpp"

namespace cppa {

/**
 * @brief A cooperatively scheduled, event-based actor implementation.
 *
 * This is the recommended base class for user-defined actors and is used
 * implicitly when spawning functor-based actors without the
 * {@link blocking_api} flag.
 *
 * @extends local_actor
 */
class event_based_actor
    : public extend<local_actor, event_based_actor>::with<
          mixin::mailbox_based, mixin::behavior_stack_based<behavior>::impl,
          mixin::sync_sender<nonblocking_response_handle_tag>::impl> {

 public:

    event_based_actor();

    ~event_based_actor();

    class functor_based;

 protected:

    /**
     * @brief Returns the initial actor behavior.
     */
    virtual behavior make_behavior() = 0;

    /**
     * @brief Forwards the last received message to @p whom.
     */
    void forward_to(const actor& whom);

    bool m_initialized;

};

class event_based_actor::functor_based
    : public extend<event_based_actor>::with<mixin::functor_based> {

    using super = combined_type;

 public:

    template<typename... Ts>
    functor_based(Ts&&... vs)
            : super(std::forward<Ts>(vs)...) {}

    behavior make_behavior() override;

};

} // namespace cppa

#endif // CPPA_UNTYPED_ACTOR_HPP
