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

#ifndef CAF_CONTEXT_HPP
#define CAF_CONTEXT_HPP

#include <atomic>
#include <cstdint>
#include <functional>

#include "caf/actor.hpp"
#include "caf/extend.hpp"
#include "caf/message.hpp"
#include "caf/channel.hpp"
#include "caf/duration.hpp"
#include "caf/behavior.hpp"
#include "caf/spawn_fwd.hpp"
#include "caf/message_id.hpp"
#include "caf/match_expr.hpp"
#include "caf/exit_reason.hpp"
#include "caf/typed_actor.hpp"
#include "caf/spawn_options.hpp"
#include "caf/abstract_actor.hpp"
#include "caf/abstract_group.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/message_handler.hpp"
#include "caf/response_promise.hpp"
#include "caf/message_priority.hpp"

#include "caf/mixin/memory_cached.hpp"

#include "caf/detail/behavior_stack.hpp"
#include "caf/detail/typed_actor_util.hpp"
#include "caf/detail/single_reader_queue.hpp"

namespace caf {

// forward declarations
class sync_handle_helper;

/**
 * @brief Base class for local running Actors.
 * @extends abstract_actor
 */
class local_actor : public extend<abstract_actor>::with<mixin::memory_cached> {

    using super = combined_type;

 public:

    using del = detail::disposer;

    using mailbox_type = detail::single_reader_queue<mailbox_element, del>;

    ~local_actor();

    /**************************************************************************
     *                          spawn untyped actors                          *
     **************************************************************************/

    template<class C, spawn_options Os = no_spawn_options, typename... Ts>
    actor spawn(Ts&&... args) {
        constexpr auto os = make_unbound(Os);
        auto res = spawn_class<C, os>(m_host, empty_before_launch_callback{},
                                      std::forward<Ts>(args)...);
        return eval_opts(Os, std::move(res));
    }

    template<spawn_options Os = no_spawn_options, typename... Ts>
    actor spawn(Ts&&... args) {
        constexpr auto os = make_unbound(Os);
        auto res = spawn_functor<os>(m_host, empty_before_launch_callback{},
                                     std::forward<Ts>(args)...);
        return eval_opts(Os, std::move(res));
    }

    template<class C, spawn_options Os, typename... Ts>
    actor spawn_in_group(const group& grp, Ts&&... args) {
        constexpr auto os = make_unbound(Os);
        auto res = spawn_class<C, os>(m_host, group_subscriber{grp},
                                      std::forward<Ts>(args)...);
        return eval_opts(Os, std::move(res));
    }

    template<spawn_options Os = no_spawn_options, typename... Ts>
    actor spawn_in_group(const group& grp, Ts&&... args) {
        constexpr auto os = make_unbound(Os);
        auto res = spawn_functor<os>(m_host, group_subscriber{grp},
                                     std::forward<Ts>(args)...);
        return eval_opts(Os, std::move(res));
    }

    /**************************************************************************
     *                           spawn typed actors                           *
     **************************************************************************/

    template<class C, spawn_options Os = no_spawn_options, typename... Ts>
    typename detail::actor_handle_from_signature_list<
        typename C::signatures>::type
    spawn_typed(Ts&&... args) {
        constexpr auto os = make_unbound(Os);
        auto res = spawn_class<C, os>(m_host, empty_before_launch_callback{},
                                      std::forward<Ts>(args)...);
        return eval_opts(Os, std::move(res));
    }

    template<spawn_options Os = no_spawn_options, typename F, typename... Ts>
    typename detail::infer_typed_actor_handle<
        typename detail::get_callable_trait<F>::result_type,
        typename detail::tl_head<
            typename detail::get_callable_trait<F>::arg_types>::type>::type
    spawn_typed(F fun, Ts&&... args) {
        constexpr auto os = make_unbound(Os);
        auto res = caf::spawn_typed_functor<os>(
            m_host, empty_before_launch_callback{}, std::move(fun),
            std::forward<Ts>(args)...);
        return eval_opts(Os, std::move(res));
    }

    /**************************************************************************
     *                       send asynchronous messages                       *
     **************************************************************************/

    /**
     * @brief Sends @p what to the receiver specified in @p dest.
     */
    void send_tuple(message_priority prio, const channel& whom, message what);

    /**
     * @brief Sends @p what to the receiver specified in @p dest.
     */
    inline void send_tuple(const channel& whom, message what) {
        send_tuple(message_priority::normal, whom, std::move(what));
    }

    /**
     * @brief Sends <tt>{what...}</tt> to @p whom.
     * @param prio Priority of the message.
     * @param whom Receiver of the message.
     * @param what Message elements.
     * @pre <tt>sizeof...(Ts) > 0</tt>.
     */
    template<typename... Ts>
    inline void send(message_priority prio, const channel& whom, Ts&&... what) {
        static_assert(sizeof...(Ts) > 0, "sizeof...(Ts) == 0");
        send_tuple(prio, whom, make_message(std::forward<Ts>(what)...));
    }

    /**
     * @brief Sends <tt>{what...}</tt> to @p whom.
     * @param whom Receiver of the message.
     * @param what Message elements.
     * @pre <tt>sizeof...(Ts) > 0</tt>.
     */
    template<typename... Ts>
    inline void send(const channel& whom, Ts&&... what) {
        static_assert(sizeof...(Ts) > 0, "sizeof...(Ts) == 0");
        send_tuple(message_priority::normal, whom,
                   make_message(std::forward<Ts>(what)...));
    }

    /**
     * @brief Sends <tt>{what...}</tt> to @p whom.
     * @param whom Receiver of the message.
     * @param what Message elements.
     * @pre <tt>sizeof...(Ts) > 0</tt>.
     */
    template<typename... Rs, typename... Ts>
    void send(const typed_actor<Rs...>& whom, Ts... what) {
        check_typed_input(
            whom, detail::type_list<typename detail::implicit_conversions<
                      typename detail::rm_const_and_ref<Ts>::type>::type...>{});
        send_tuple(message_priority::normal, actor{whom.m_ptr.get()},
                   make_message(std::forward<Ts>(what)...));
    }

    /**
     * @brief Sends an exit message to @p whom.
     */
    void send_exit(const actor_addr& whom, uint32_t reason);

    /**
     * @brief Sends an exit message to @p whom.
     */
    inline void send_exit(const actor& whom, uint32_t reason) {
        send_exit(whom.address(), reason);
    }

    /**
     * @brief Sends an exit message to @p whom.
     */
    template<typename... Rs>
    void send_exit(const typed_actor<Rs...>& whom, uint32_t reason) {
        send_exit(whom.address(), reason);
    }

    /**
     * @brief Sends a message to @p whom that is delayed by @p rel_time.
     * @param prio Priority of the message.
     * @param whom Receiver of the message.
     * @param rtime Relative time to delay the message in
     *              microseconds, milliseconds, seconds or minutes.
     * @param data Message content as a tuple.
     */
    void delayed_send_tuple(message_priority prio, const channel& whom,
                            const duration& rtime, message data);

    /**
     * @brief Sends a message to @p whom that is delayed by @p rel_time.
     * @param whom Receiver of the message.
     * @param rtime Relative time to delay the message in
     *              microseconds, milliseconds, seconds or minutes.
     * @param data Message content as a tuple.
     */
    inline void delayed_send_tuple(const channel& whom, const duration& rtime,
                                   message data) {
        delayed_send_tuple(message_priority::normal, whom, rtime,
                           std::move(data));
    }

    /**
     * @brief Sends a message to @p whom that is delayed by @p rel_time.
     * @param prio Priority of the message.
     * @param whom Receiver of the message.
     * @param rtime Relative time to delay the message in
     *              microseconds, milliseconds, seconds or minutes.
     * @param args Message content as a tuple.
     */
    template<typename... Ts>
    void delayed_send(message_priority prio, const channel& whom,
                      const duration& rtime, Ts&&... args) {
        delayed_send_tuple(prio, whom, rtime,
                           make_message(std::forward<Ts>(args)...));
    }

    /**
     * @brief Sends a message to @p whom that is delayed by @p rel_time.
     * @param whom Receiver of the message.
     * @param rtime Relative time to delay the message in
     *              microseconds, milliseconds, seconds or minutes.
     * @param args Message content as a tuple.
     */
    template<typename... Ts>
    void delayed_send(const channel& whom, const duration& rtime,
                      Ts&&... args) {
        delayed_send_tuple(message_priority::normal, whom, rtime,
                           make_message(std::forward<Ts>(args)...));
    }

    /**************************************************************************
     *                     miscellaneous actor operations                     *
     **************************************************************************/

    /**
     * @brief Causes this actor to subscribe to the group @p what.
     *
     * The group will be unsubscribed if the actor finishes execution.
     * @param what Group instance that should be joined.
     */
    void join(const group& what);

    /**
     * @brief Causes this actor to leave the group @p what.
     * @param what Joined group that should be leaved.
     * @note Groups are leaved automatically if the Actor finishes
     *       execution.
     */
    void leave(const group& what);

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
     * @warning This member function throws immediately in thread-based actors
     *          that do not use the behavior stack, i.e., actors that use
     *          blocking API calls such as {@link receive()}.
     */
    virtual void quit(uint32_t reason = exit_reason::normal);

    /**
     * @brief Checks whether this actor traps exit messages.
     */
    inline bool trap_exit() const;

    /**
     * @brief Enables or disables trapping of exit messages.
     */
    inline void trap_exit(bool new_value);

    /**
     * @brief Returns the last message that was dequeued
     *        from the actor's mailbox.
     * @warning Only set during callback invocation.
     */
    inline message& last_dequeued();

    /**
     * @brief Returns the address of the last sender of the
     *        last dequeued message.
     */
    inline actor_addr& last_sender();

    /**
     * @brief Adds a unidirectional @p monitor to @p whom.
     * @param whom The actor that should be monitored by this actor.
     * @note Each call to @p monitor creates a new, independent monitor.
     */
    void monitor(const actor_addr& whom);

    /**
     * @copydoc monitor(const actor_addr&)
     */
    inline void monitor(const actor& whom) { monitor(whom.address()); }

    /**
     * @copydoc monitor(const actor_addr&)
     */
    template<typename... Rs>
    inline void monitor(const typed_actor<Rs...>& whom) {
        monitor(whom.address());
    }

    /**
     * @brief Removes a monitor from @p whom.
     * @param whom A monitored actor.
     */
    void demonitor(const actor_addr& whom);

    /**
     * @brief Removes a monitor from @p whom.
     * @param whom A monitored actor.
     */
    inline void demonitor(const actor& whom) { demonitor(whom.address()); }

    /**
     * @brief Can be overridden to perform cleanup code after an actor
     *        finished execution.
     */
    virtual void on_exit();

    /**
     * @brief Returns all joined groups of this actor.
     */
    std::vector<group> joined_groups() const;

    /**
     * @brief Creates a {@link response_promise} to allow actors to response
     *        to a request later on.
     */
    response_promise make_response_promise();

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

    /**************************************************************************
     *                here be dragons: end of public interface                *
     **************************************************************************/

    /** @cond PRIVATE */

    local_actor();

    template<class ActorHandle>
    inline ActorHandle eval_opts(spawn_options opts, ActorHandle res) {
        if (has_monitor_flag(opts)) {
            monitor(res->address());
        }
        if (has_link_flag(opts)) {
            link_to(res->address());
        }
        return res;
    }

    inline void current_node(mailbox_element* ptr) {
        this->m_current_node = ptr;
    }

    inline mailbox_element* current_node() { return this->m_current_node; }

    inline message_id new_request_id() {
        auto result = ++m_last_request_id;
        m_pending_responses.push_back(result.response_id());
        return result;
    }

    inline void handle_sync_timeout() {
        if (m_sync_timeout_handler)
            m_sync_timeout_handler();
        else
            quit(exit_reason::unhandled_sync_timeout);
    }

    inline void handle_sync_failure() {
        if (m_sync_failure_handler)
            m_sync_failure_handler();
        else
            quit(exit_reason::unhandled_sync_failure);
    }

    // returns the response ID
    message_id timed_sync_send_tuple_impl(message_priority mp,
                                          const actor& whom,
                                          const duration& rel_time,
                                          message&& what);

    // returns the response ID
    message_id sync_send_tuple_impl(message_priority mp, const actor& whom,
                                    message&& what);

    // returns the response ID
    template<typename... Rs, typename... Ts>
    message_id sync_send_tuple_impl(message_priority mp,
                                    const typed_actor<Rs...>& whom,
                                    message&& msg) {
        return sync_send_tuple_impl(mp, actor{whom.m_ptr.get()},
                                    std::move(msg));
    }

    // returns 0 if last_dequeued() is an asynchronous or sync request message,
    // a response id generated from the request id otherwise
    inline message_id get_response_id();

    void reply_message(message&& what);

    void forward_message(const actor& new_receiver, message_priority prio);

    inline bool awaits(message_id response_id);

    inline void mark_arrived(message_id response_id);

    inline uint32_t planned_exit_reason() const;

    inline void planned_exit_reason(uint32_t value);

    void cleanup(uint32_t reason) override;

    mailbox_element* dummy_node() { return &m_dummy_node; }

    virtual optional<behavior&> sync_handler(message_id msg_id) = 0;

 protected:

    template<typename... Rs, template<typename...> class T, typename... Ts>
    static void check_typed_input(const typed_actor<Rs...>&, const T<Ts...>&) {
        static constexpr int input_pos = detail::tl_find_if<
            detail::type_list<Rs...>,
            detail::input_is<detail::type_list<Ts...>>::template eval>::value;
        static_assert(input_pos >= 0,
                      "typed actor does not support given input");
    }

    template<typename... Ts>
    inline mailbox_element* new_mailbox_element(Ts&&... args) {
        return mailbox_element::create(std::forward<Ts>(args)...);
    }

    // true if this actor receives EXIT messages as ordinary messages
    bool m_trap_exit;

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
    std::map<group, abstract_group::subscription> m_subscriptions;

    // set by quit
    uint32_t m_planned_exit_reason;

    /** @endcond */

 private:

    std::function<void()> m_sync_failure_handler;
    std::function<void()> m_sync_timeout_handler;

};

/**
 * @brief A smart pointer to a {@link local_actor} instance.
 * @relates local_actor
 */
using local_actor_ptr = intrusive_ptr<local_actor>;

/******************************************************************************
 *             inline and template member function implementations            *
 ******************************************************************************/

/** @cond PRIVATE */

inline bool local_actor::trap_exit() const { return m_trap_exit; }

inline void local_actor::trap_exit(bool new_value) { m_trap_exit = new_value; }

inline message& local_actor::last_dequeued() { return m_current_node->msg; }

inline actor_addr& local_actor::last_sender() { return m_current_node->sender; }

inline message_id local_actor::get_response_id() {
    auto id = m_current_node->mid;
    return (id.is_request()) ? id.response_id() : message_id();
}

inline bool local_actor::awaits(message_id response_id) {
    CAF_REQUIRE(response_id.is_response());
    return std::any_of(m_pending_responses.begin(), m_pending_responses.end(),
                       [=](message_id other) { return response_id == other; });
}

inline void local_actor::mark_arrived(message_id response_id) {
    auto last = m_pending_responses.end();
    auto i = std::find(m_pending_responses.begin(), last, response_id);
    if (i != last) m_pending_responses.erase(i);
}

inline uint32_t local_actor::planned_exit_reason() const {
    return m_planned_exit_reason;
}

inline void local_actor::planned_exit_reason(uint32_t value) {
    m_planned_exit_reason = value;
}

/** @endcond */

} // namespace caf

#endif // CAF_CONTEXT_HPP
