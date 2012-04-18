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

#include "cppa/config.hpp"
#include "cppa/either.hpp"
#include "cppa/pattern.hpp"
#include "cppa/behavior.hpp"
#include "cppa/detail/disablable_delete.hpp"
#include "cppa/detail/abstract_scheduled_actor.hpp"

namespace cppa {

struct vec_append
{
    inline std::vector<detail::recursive_queue_node>::iterator
    operator()(std::vector<detail::recursive_queue_node>& result,
               detail::recursive_queue_node* e) const
    {
        std::vector<std::unique_ptr<detail::recursive_queue_node> > tmp;
        while (e)
        {
            auto next = e->next;
            tmp.emplace_back(e);
            e = next;
        }
        auto old_size = result.size();
        for (auto i = tmp.rbegin(); i != tmp.rend(); ++i)
        {
            result.emplace_back(std::move(*(*i)));
        }
        return result.begin() + old_size;
    }
};

/**
 * @brief Base class for all event-based actor implementations.
 */
class abstract_event_based_actor
        : public detail::abstract_scheduled_actor<
            intrusive::single_reader_queue<
                detail::recursive_queue_node,
                std::vector<detail::recursive_queue_node>,
                vec_append
            >
          >
{

    typedef detail::abstract_scheduled_actor<
                intrusive::single_reader_queue<
                    detail::recursive_queue_node,
                    std::vector<detail::recursive_queue_node>,
                    vec_append
                >
            >
            super;

 public:

    void dequeue(behavior&); //override

    void dequeue(partial_function&); //override

    void resume(util::fiber*, scheduler::callback* cb); //override

    /**
     * @brief Initializes the actor by defining an initial behavior.
     */
    virtual void init() = 0;

    /**
     * @copydoc cppa::scheduled_actor::on_exit()
     */
    virtual void on_exit();

 protected:

    abstract_event_based_actor();

    // ownership flag + pointer
    typedef std::unique_ptr<behavior, detail::disablable_delete<behavior>>
            stack_element;

    std::vector<stack_element> m_loop_stack;

    // provoke compiler errors for usage of receive() and related functions

    /**
     * @brief Provokes a compiler error to ensure that an event-based actor
     *        does not accidently uses receive() instead of become().
     */
    template<typename... Args>
    void receive(Args&&...)
    {
        static_assert((sizeof...(Args) + 1) < 1,
                      "You shall not use receive in an event-based actor. "
                      "Use become() instead.");
    }

    /**
     * @brief Provokes a compiler error.
     */
    template<typename... Args>
    void receive_loop(Args&&... args)
    {
        receive(std::forward<Args>(args)...);
    }

    /**
     * @brief Provokes a compiler error.
     */
    template<typename... Args>
    void receive_while(Args&&... args)
    {
        receive(std::forward<Args>(args)...);
    }

    /**
     * @brief Provokes a compiler error.
     */
    template<typename... Args>
    void do_receive(Args&&... args)
    {
        receive(std::forward<Args>(args)...);
    }

 private:

    bool handle_message(mailbox_element& iter);

};

} // namespace cppa

#endif // EVENT_DRIVEN_ACTOR_HPP
