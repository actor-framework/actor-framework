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
 * Copyright (C) 2011, 2012                                                   *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation, either version 3 of the License                  *
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


#ifndef SCHEDULED_ACTOR_HPP
#define SCHEDULED_ACTOR_HPP

#include "cppa/scheduler.hpp"
#include "cppa/local_actor.hpp"
#include "cppa/abstract_actor.hpp"
#include "cppa/scheduled_actor.hpp"

#include "cppa/util/fiber.hpp"

#include "cppa/intrusive/singly_linked_list.hpp"
#include "cppa/intrusive/single_reader_queue.hpp"

namespace cppa { class scheduler; }

namespace cppa { namespace detail {

// A spawned, scheduled Actor.
class abstract_scheduled_actor : public abstract_actor<local_actor>
{

    friend class intrusive::single_reader_queue<abstract_scheduled_actor>;

    abstract_scheduled_actor* next; // intrusive next pointer

    void enqueue_node(queue_node* node);

 protected:

    std::atomic<int> m_state;
    scheduler* m_scheduler;

    typedef abstract_actor super;
    typedef super::queue_node_guard queue_node_guard;
    typedef super::queue_node queue_node;
    typedef super::queue_node_ptr queue_node_ptr;

    enum dq_result
    {
        dq_done,
        dq_indeterminate,
        dq_timeout_occured
    };

    enum filter_result
    {
        normal_exit_signal,
        expired_timeout_message,
        timeout_message,
        ordinary_message
    };

    filter_result filter_msg(const any_tuple& msg);

    auto dq(queue_node& node, partial_function& rules) -> dq_result;

    bool has_pending_timeout()
    {
        return m_has_pending_timeout_request;
    }

    void request_timeout(const util::duration& d);

    void reset_timeout()
    {
        if (m_has_pending_timeout_request)
        {
            ++m_active_timeout_id;
            m_has_pending_timeout_request = false;
        }
    }

 private:

    bool m_has_pending_timeout_request;
    std::uint32_t m_active_timeout_id;

 public:

    static constexpr int ready          = 0x00;
    static constexpr int done           = 0x01;
    static constexpr int blocked        = 0x02;
    static constexpr int about_to_block = 0x04;

    abstract_scheduled_actor(int state = done);

    abstract_scheduled_actor(scheduler* sched);

    void quit(std::uint32_t reason);

    void enqueue(actor* sender, any_tuple&& msg);

    void enqueue(actor* sender, const any_tuple& msg);

    int compare_exchange_state(int expected, int new_value);

    struct resume_callback
    {
        virtual ~resume_callback();
        // called if an actor finished execution
        virtual void exec_done() = 0;
    };

    // from = calling worker
    virtual void resume(util::fiber* from, resume_callback* callback) = 0;

};

struct scheduled_actor_dummy : abstract_scheduled_actor
{
    void resume(util::fiber*, resume_callback*);
    void quit(std::uint32_t);
    void dequeue(behavior&);
    void dequeue(partial_function&);
    void link_to(intrusive_ptr<actor>&);
    void unlink_from(intrusive_ptr<actor>&);
    bool establish_backlink(intrusive_ptr<actor>&);
    bool remove_backlink(intrusive_ptr<actor>&);
    void detach(const attachable::token&);
    bool attach(attachable*);
};

} } // namespace cppa::detail

#endif // SCHEDULED_ACTOR_HPP
