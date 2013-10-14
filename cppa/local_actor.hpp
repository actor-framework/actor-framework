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
 * Copyright (C) 2011-2013                                                    *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation; either version 2.1 of the License,               *
 * or (at your option) any later version.                                     *
 *                                                                            *
 * libcppa is distributed in the hope that it will be useful,                 *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                       *
 * See the GNU Lesser General Public License for more details.                *
 *                                                                            *
 * You should have received a copy of the GNU Lesser General Public License   *
 * along with libcppa. If not, see <http://www.gnu.org/licenses/>.            *
\******************************************************************************/


#ifndef CPPA_CONTEXT_HPP
#define CPPA_CONTEXT_HPP

#include <cstdint>
#include <functional>

#include "cppa/group.hpp"
#include "cppa/actor.hpp"
#include "cppa/extend.hpp"
#include "cppa/behavior.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/message_id.hpp"
#include "cppa/match_expr.hpp"
#include "cppa/exit_reason.hpp"
#include "cppa/memory_cached.hpp"
#include "cppa/message_header.hpp"
#include "cppa/mailbox_element.hpp"
#include "cppa/response_handle.hpp"
#include "cppa/message_priority.hpp"
#include "cppa/partial_function.hpp"

#include "cppa/util/duration.hpp"

#include "cppa/detail/behavior_stack.hpp"

namespace cppa {

// forward declarations
class self_type;
class scheduler;
class message_future;
class local_scheduler;
class sync_handle_helper;

#ifdef CPPA_DOCUMENTATION

/**
 * @brief Policy tag that causes {@link event_based_actor::become} to
 *        discard the current behavior.
 * @relates local_actor
 */
constexpr auto discard_behavior;

/**
 * @brief Policy tag that causes {@link event_based_actor::become} to
 *        keep the current behavior available.
 * @relates local_actor
 */
constexpr auto keep_behavior;

#else

template<bool DiscardBehavior>
struct behavior_policy { static constexpr bool discard_old = DiscardBehavior; };

template<typename T>
struct is_behavior_policy : std::false_type { };

template<bool DiscardBehavior>
struct is_behavior_policy<behavior_policy<DiscardBehavior>> : std::true_type { };

typedef behavior_policy<false> keep_behavior_t;
typedef behavior_policy<true > discard_behavior_t;

constexpr discard_behavior_t discard_behavior = discard_behavior_t{};

constexpr keep_behavior_t keep_behavior = keep_behavior_t{};

#endif

/**
 * @brief Base class for local running Actors.
 * @extends actor
 */
class local_actor : public extend<actor>::with<memory_cached> {

    friend class self_type;

    typedef combined_type super;

 public:

    ~local_actor();

    /**
     * @brief Causes this actor to subscribe to the group @p what.
     *
     * The group will be unsubscribed if the actor finishes execution.
     * @param what Group instance that should be joined.
     */
    void join(const group_ptr& what);

    /**
     * @brief Causes this actor to leave the group @p what.
     * @param what Joined group that should be leaved.
     * @note Groups are leaved automatically if the Actor finishes
     *       execution.
     */
    void leave(const group_ptr& what);

    /**
     * @brief Finishes execution of this actor after any currently running
     *        message handler is done.
     *
     * This member function clear the behavior stack of the running actor
     * and invokes {@link on_exit()}. The actors does not finish execution
     * if the implementation of {@link on_exit()} sets a new behavior.
     * When setting a new behavior in {@link on_exit()}, one has to make sure
     * to not produce an infinite recursion.
     *
     * If {@link on_exit()} did not set a new behavior, the actor sends an
     * exit message to all of its linked actors, sets its state to @c exited
     * and finishes execution.
     *
     * @param reason Exit reason that will be send to
     *               linked actors and monitors. Can be queried using
     *               {@link planned_exit_reason()}, e.g., from inside
     *               {@link on_exit()}.
     * @note Throws {@link actor_exited} to unwind the stack
     *       when called in context-switching or thread-based actors.
     * @warning This member function throws imeediately in thread-based actors
     *          that do not use the behavior stack, i.e., actors that use
     *          blocking API calls such as {@link receive()}.
     */
    virtual void quit(std::uint32_t reason = exit_reason::normal);

    /**
     * @brief Checks whether this actor traps exit messages.
     */
    inline bool trap_exit() const;

    /**
     * @brief Enables or disables trapping of exit messages.
     */
    inline void trap_exit(bool new_value);

    /**
     * @brief Checks whether this actor uses the "chained send" optimization.
     */
    inline bool chaining() const;

    /**
     * @brief Enables or disables chained send.
     */
    inline void chaining(bool new_value);

    /**
     * @brief Returns the last message that was dequeued
     *        from the actor's mailbox.
     * @warning Only set during callback invocation.
     */
    inline any_tuple& last_dequeued();

    /**
     * @brief Returns the sender of the last dequeued message.
     * @warning Only set during callback invocation.
     */
    inline actor_ptr& last_sender();

    /**
     * @brief Adds a unidirectional @p monitor to @p whom.
     *
     * @whom sends a "DOWN" message to this actor as part of its termination.
     * @param whom The actor that should be monitored by this actor.
     * @note Each call to @p monitor creates a new, independent monitor.
     */
    void monitor(const actor_ptr& whom);

    /**
     * @brief Removes a monitor from @p whom.
     * @param whom A monitored actor.
     */
    void demonitor(const actor_ptr& whom);

    /**
     * @brief Can be overridden to initialize an actor before any
     *        message is handled.
     * @warning Must not call blocking functions such as
     *          {@link cppa::receive receive}.
     * @note Calling {@link become} to set an initial behavior is supported.
     */
    virtual void init();

    /**
     * @brief Can be overridden to perform cleanup code after an actor
     *        finished execution.
     * @warning Must not call any function manipulating the actor's state such
     *          as join, leave, link, or monitor.
     */
    virtual void on_exit();

    /**
     * @brief Returns all joined groups of this actor.
     */
    std::vector<group_ptr> joined_groups() const;

    /**
     * @ingroup BlockingAPI
     * @brief Executes an actor's behavior stack until it is empty.
     *
     * This member function allows thread-based and context-switching actors
     * to make use of the nonblocking behavior API of libcppa.
     * Calling this member function in an event-based actor does nothing.
     */
    virtual void exec_behavior_stack();

    /**
     * @brief Creates a {@link response_handle} to allow actors to response
     *        to a request later on.
     */
    response_handle make_response_handle();

    /**
     * @brief Sets the handler for unexpected synchronous response messages.
     */
    inline void on_sync_timeout(std::function<void()> fun) {
        m_sync_timeout_handler = std::move(fun);
    }

    /**
     * @brief Sets the handler for @p timed_sync_send timeout messages.
     */
    inline void on_sync_failure(std::function<void()> fun) {
        m_sync_failure_handler = std::move(fun);
    }

    /**
     * @brief Checks wheter this actor has a user-defined sync failure handler.
     */
    inline bool has_sync_failure_handler() {
        return static_cast<bool>(m_sync_failure_handler);
    }

    /**
     * @brief Calls <tt>on_sync_timeout(fun); on_sync_failure(fun);</tt>.
     */
    inline void on_sync_timeout_or_failure(std::function<void()> fun) {
        on_sync_timeout(fun);
        on_sync_failure(fun);
    }

    /** @cond PRIVATE */

    inline message_id new_request_id() {
        auto result = ++m_last_request_id;
        m_pending_responses.push_back(result.response_id());
        return result;
    }

    inline void handle_sync_timeout() {
        if (m_sync_timeout_handler) m_sync_timeout_handler();
        else quit(exit_reason::unhandled_sync_timeout);
    }

    inline void handle_sync_failure() {
        if (m_sync_failure_handler) m_sync_failure_handler();
        else quit(exit_reason::unhandled_sync_failure);
    }

    virtual void dequeue(behavior& bhvr);

    inline void dequeue(behavior&& bhvr);

    virtual void dequeue_response(behavior&, message_id);

    inline void dequeue_response(behavior&&, message_id);

    inline void do_unbecome();

    local_actor(bool is_scheduled = false);

    virtual bool initialized() const = 0;

    inline bool chaining_enabled();

    message_id send_timed_sync_message(message_priority mp,
                                       const actor_ptr& whom,
                                       const util::duration& rel_time,
                                       any_tuple&& what);

    // returns 0 if last_dequeued() is an asynchronous or sync request message,
    // a response id generated from the request id otherwise
    inline message_id get_response_id();

    void reply_message(any_tuple&& what);

    void forward_message(const actor_ptr& new_receiver, message_priority prio);

    inline const actor_ptr& chained_actor();

    inline void chained_actor(const actor_ptr& value);

    inline bool awaits(message_id response_id);

    inline void mark_arrived(message_id response_id);

    virtual void become_waiting_for(behavior, message_id) = 0;

    inline detail::behavior_stack& bhvr_stack();

    virtual void do_become(behavior&& bhvr, bool discard_old) = 0;

    inline void do_become(const behavior& bhvr, bool discard_old);

    const char* debug_name() const;

    void debug_name(std::string str);

    inline std::uint32_t planned_exit_reason() const;

    inline void planned_exit_reason(std::uint32_t value);

 protected:

    inline void remove_handler(message_id id);

    void cleanup(std::uint32_t reason);

    // used *only* when compiled in debug mode
    union { std::string m_debug_name; };

    // true if this actor uses the chained_send optimization
    bool m_chaining;

    // true if this actor receives EXIT messages as ordinary messages
    bool m_trap_exit;

    // true if this actor takes part in cooperative scheduling
    bool m_is_scheduled;

    // pointer to the actor that is marked as successor due to a chained send
    actor_ptr m_chained_actor;

    // identifies the ID of the last sent synchronous request
    message_id m_last_request_id;

    // identifies all IDs of sync messages waiting for a response
    std::vector<message_id> m_pending_responses;

    // "default value" for m_current_node
    mailbox_element m_dummy_node;

    // points to m_dummy_node if no callback is currently invoked,
    // points to the node under processing otherwise
    mailbox_element* m_current_node;

    // {group => subscription} map of all joined groups
    std::map<group_ptr, group::subscription> m_subscriptions;

    // allows actors to keep previous behaviors and enables unbecome()
    detail::behavior_stack m_bhvr_stack;

    // set by quit
    std::uint32_t m_planned_exit_reason;

    /** @endcond */

 private:

    std::function<void()> m_sync_failure_handler;
    std::function<void()> m_sync_timeout_handler;

};

/**
 * @brief A smart pointer to a {@link local_actor} instance.
 * @relates local_actor
 */
typedef intrusive_ptr<local_actor> local_actor_ptr;


/******************************************************************************
 *             inline and template member function implementations            *
 ******************************************************************************/

/** @cond PRIVATE */

inline void local_actor::dequeue(behavior&& bhvr) {
    behavior tmp{std::move(bhvr)};
    dequeue(tmp);
}

inline void local_actor::dequeue_response(behavior&& bhvr, message_id id) {
    behavior tmp{std::move(bhvr)};
    dequeue_response(tmp, id);
}

inline bool local_actor::trap_exit() const {
    return m_trap_exit;
}

inline void local_actor::trap_exit(bool new_value) {
    m_trap_exit = new_value;
}

inline bool local_actor::chaining() const {
    return m_chaining;
}

inline void local_actor::chaining(bool new_value) {
    m_chaining = m_is_scheduled && new_value;
}

inline any_tuple& local_actor::last_dequeued() {
    return m_current_node->msg;
}

inline actor_ptr& local_actor::last_sender() {
    return m_current_node->sender;
}

inline void local_actor::do_unbecome() {
    m_bhvr_stack.pop_async_back();
}

inline bool local_actor::chaining_enabled() {
    return m_chaining && !m_chained_actor;
}

inline message_id local_actor::get_response_id() {
    auto id = m_current_node->mid;
    return (id.is_request()) ? id.response_id() : message_id();
}

inline const actor_ptr& local_actor::chained_actor() {
    return m_chained_actor;
}

inline void local_actor::chained_actor(const actor_ptr& value) {
    m_chained_actor = value;
}

inline bool local_actor::awaits(message_id response_id) {
    CPPA_REQUIRE(response_id.is_response());
    return std::any_of(m_pending_responses.begin(),
                       m_pending_responses.end(),
                       [=](message_id other) {
                           return response_id == other;
                       });
}

inline void local_actor::mark_arrived(message_id response_id) {
    auto last = m_pending_responses.end();
    auto i = std::find(m_pending_responses.begin(), last, response_id);
    if (i != last) m_pending_responses.erase(i);
}

inline detail::behavior_stack& local_actor::bhvr_stack() {
    return m_bhvr_stack;
}

inline void local_actor::do_become(const behavior& bhvr, bool discard_old) {
    behavior copy{bhvr};
    do_become(std::move(copy), discard_old);
}

inline void local_actor::remove_handler(message_id id) {
    m_bhvr_stack.erase(id);
}

inline std::uint32_t local_actor::planned_exit_reason() const {
    return m_planned_exit_reason;
}

inline void local_actor::planned_exit_reason(std::uint32_t value) {
    m_planned_exit_reason = value;
}

/** @endcond */

} // namespace cppa

#endif // CPPA_CONTEXT_HPP
