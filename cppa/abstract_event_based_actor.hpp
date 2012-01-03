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


#ifndef EVENT_DRIVEN_ACTOR_HPP
#define EVENT_DRIVEN_ACTOR_HPP

#include <stack>
#include <memory>
#include <vector>

#include "cppa/pattern.hpp"
#include "cppa/invoke_rules.hpp"
#include "cppa/util/either.hpp"
#include "cppa/detail/abstract_scheduled_actor.hpp"

namespace cppa {

class abstract_event_based_actor : public detail::abstract_scheduled_actor
{

    typedef detail::abstract_scheduled_actor super;
    typedef super::queue_node queue_node;
    typedef super::queue_node_buffer queue_node_buffer;

 public:

    void dequeue(invoke_rules&) /*override*/;

    void dequeue(timed_invoke_rules&) /*override*/;

    void resume(util::fiber*, resume_callback* callback) /*override*/;

    /**
     *
     */
    virtual void init() = 0;

    /**
     *
     */
    virtual void on_exit();

    template<typename Scheduler>
    abstract_event_based_actor*
    attach_to_scheduler(void (*enqueue_fun)(Scheduler*, abstract_scheduled_actor*),
                        Scheduler* sched)
    {
        m_enqueue_to_scheduler.reset(enqueue_fun, sched, this);
        this->init();
        return this;
    }

 private:

    void handle_message(std::unique_ptr<queue_node>& node,
                        invoke_rules& behavior);

    void handle_message(std::unique_ptr<queue_node>& node,
                        timed_invoke_rules& behavior);

    void handle_message(std::unique_ptr<queue_node>& node);

 protected:

    struct stack_element
    {

        stack_element() = delete;
        stack_element(stack_element const&) = delete;
        stack_element& operator=(stack_element const&) = delete;

        util::either<invoke_rules*, timed_invoke_rules*> m_ptr;
        bool m_ownership;

        inline stack_element(invoke_rules* ptr, bool take_ownership)
            : m_ptr(ptr), m_ownership(take_ownership)
        {
        }

        inline stack_element(timed_invoke_rules* ptr, bool take_ownership)
            : m_ptr(ptr), m_ownership(take_ownership)
        {
        }

        inline stack_element(stack_element&& other)
            : m_ptr(other.m_ptr), m_ownership(other.m_ownership)
        {
            other.m_ownership = false;
        }

        inline stack_element& operator=(stack_element&& other)
        {
            m_ptr = other.m_ptr;
            m_ownership = other.m_ownership;
            other.m_ownership = false;
            return *this;
        }

        inline ~stack_element()
        {
            if (m_ownership)
            {
                if (m_ptr.is_left())
                {
                    delete m_ptr.left();
                }
                else
                {
                    delete m_ptr.right();
                }
            }
        }
        inline bool is_left()
        {
            return m_ptr.is_left();
        }
        inline bool is_right()
        {
            return m_ptr.is_right();
        }
        inline invoke_rules& left()
        {
            auto ptr = m_ptr.left();
            if (ptr == nullptr) throw std::runtime_error("nullptr");
            return *(ptr);
        }
        inline timed_invoke_rules& right()
        {
            auto ptr = m_ptr.right();
            if (ptr == nullptr) throw std::runtime_error("nullptr");
            return *(ptr);
        }
    };

    queue_node_buffer m_buffer;
    std::stack<stack_element, std::vector<stack_element> > m_loop_stack;

    // provoke compiler errors for usage of receive() and related functions

    template<typename... Args>
    void receive(Args&&...)
    {
        static_assert((sizeof...(Args) + 1) < 1,
                      "You shall not use receive in an event-based actor. "
                      "Use become() instead.");
    }

    template<typename... Args>
    void receive_loop(Args&&... args)
    {
        receive(std::forward<Args>(args)...);
    }

    template<typename... Args>
    void receive_while(Args&&... args)
    {
        receive(std::forward<Args>(args)...);
    }

    template<typename... Args>
    void do_receive(Args&&... args)
    {
        receive(std::forward<Args>(args)...);
    }

};

} // namespace cppa

#endif // EVENT_DRIVEN_ACTOR_HPP
