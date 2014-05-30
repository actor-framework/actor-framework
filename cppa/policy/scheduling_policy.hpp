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


#ifndef CPPA_POLICY_SCHEDULING_POLICY_HPP
#define CPPA_POLICY_SCHEDULING_POLICY_HPP

#include "cppa/cppa_fwd.hpp"

namespace cppa {
namespace policy {

enum class timed_fetch_result {
    no_message,
    indeterminate,
    success
};

/**
 * @brief The scheduling_policy <b>concept</b> class. Please note that this
 *        class is <b>not</b> implemented. It only explains the all
 *        required member function and their behavior for any scheduling policy.
 */
class scheduling_policy {

 public:

    /**
     * @brief This typedef can be set freely by any implementation and is
     *        used by callers to pass the result of @p init_timeout back to
     *        @p fetch_messages.
     */
    using timeout_type = int;

    /**
     * @brief Fetches new messages from the actor's mailbox and feeds them
     *        to the given callback. The member function returns @p false if
     *        no message was read, @p true otherwise.
     *
     * In case this function returned @p false, the policy also sets the state
     * of the actor to blocked. Any caller must evaluate the return value and
     * act properly in case of a returned @p false, i.e., it must <b>not</b>
     * atttempt to call any further function on the actor, since it might be
     * already in the pipe for re-scheduling.
     */
    template<class Actor, typename F>
    bool fetch_messages(Actor* self, F cb);

    /**
     * @brief Tries to fetch new messages from the actor's mailbox and to feeds
     *        them to the given callback. The member function returns @p false
     *        if no message was read, @p true otherwise.
     *
     * This member function does not have any side-effect other than removing
     * messages from the actor's mailbox.
     */
    template<class Actor, typename F>
    bool try_fetch_messages(Actor* self, F cb);

    /**
     * @brief Tries to fetch new messages before a timeout occurs. The message
     *        can either return @p success, or @p no_message,
     *        or @p indeterminate. The latter occurs for cooperative scheduled
     *        operations and means that timeouts are signaled using
     *        special-purpose messages. In this case, clients have to simply
     *        wait for the arriving message.
     */
    template<class Actor, typename F>
    timed_fetch_result fetch_messages(Actor* self, F cb, timeout_type abs_time);

    /**
     * @brief Enqueues given message to the actor's mailbox and take any
     *        steps to resume the actor if it's currently blocked.
     */
    template<class Actor>
    void enqueue(Actor* self,
                 msg_hdr_cref hdr,
                 any_tuple& msg,
                 execution_unit* host);

    /**
     * @brief Starts the given actor either by launching a thread or enqueuing
     *        it to the cooperative scheduler's job queue.
     */
    template<class Actor>
    void launch(Actor* self);

};

} // namespace policy
} // namespace cppa

#endif // CPPA_POLICY_SCHEDULING_POLICY_HPP
