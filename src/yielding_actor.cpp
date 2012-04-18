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


#include "cppa/detail/yielding_actor.hpp"
#ifndef CPPA_DISABLE_CONTEXT_SWITCHING

#include <iostream>

#include "cppa/cppa.hpp"
#include "cppa/self.hpp"
#include "cppa/detail/invokable.hpp"

namespace cppa { namespace detail {

yielding_actor::yielding_actor(std::function<void()> fun)
    : m_fiber(&yielding_actor::run, this)
    , m_behavior(fun)
{
}

void yielding_actor::run(void* ptr_arg)
{
    auto this_ptr = reinterpret_cast<yielding_actor*>(ptr_arg);
    CPPA_REQUIRE(static_cast<bool>(this_ptr->m_behavior));
    bool cleanup_called = false;
    try { this_ptr->m_behavior(); }
    catch (actor_exited&)
    {
        // cleanup already called by scheduled_actor::quit
        cleanup_called = true;
    }
    catch (...)
    {
        this_ptr->cleanup(exit_reason::unhandled_exception);
        cleanup_called = true;
    }
    if (!cleanup_called) this_ptr->cleanup(exit_reason::normal);
    this_ptr->on_exit();
    yield(yield_state::done);
}


void yielding_actor::yield_until_not_empty()
{
    if (m_mailbox.can_fetch_more() == false)
    {
        m_state.store(abstract_scheduled_actor::about_to_block);
        CPPA_MEMORY_BARRIER();
        // make sure mailbox is 'empty'
        if (m_mailbox.can_fetch_more() == false)
        {
            m_state.store(abstract_scheduled_actor::ready);
            return;
        }
        else
        {
            yield(yield_state::blocked);
        }
    }
}

void yielding_actor::dequeue(partial_function& fun)
{
    auto rm_fun = [&](mailbox_cache_element& node)
    {
        return dq(*node, fun) == dq_done;
    };
    dequeue_impl(rm_fun);
}

void yielding_actor::dequeue(behavior& bhvr)
{
    if (bhvr.timeout().valid())
    {
        request_timeout(bhvr.timeout());
        auto rm_fun = [&](mailbox_cache_element& node) -> bool
        {
            switch (dq(*node, bhvr.get_partial_function()))
            {
                case dq_timeout_occured:
                    bhvr.handle_timeout();
                    return true;
                case dq_done:
                    return true;
                default:
                    return false;
            }
        };
        dequeue_impl(rm_fun);
    }
    else
    {
        // suppress virtual function call
        yielding_actor::dequeue(bhvr.get_partial_function());
    }
}

void yielding_actor::resume(util::fiber* from, scheduler::callback* callback)
{
    self.set(this);
    for (;;)
    {
        switch (call(&m_fiber, from))
        {
            case yield_state::done:
            {
                callback->exec_done();
                return;
            }
            case yield_state::ready:
            {
                break;
            }
            case yield_state::blocked:
            {
                switch (compare_exchange_state(abstract_scheduled_actor::about_to_block,
                                               abstract_scheduled_actor::blocked))
                {
                    case abstract_scheduled_actor::ready:
                    {
                        break;
                    }
                    case abstract_scheduled_actor::blocked:
                    {
                        // wait until someone re-schedules that actor
                        return;
                    }
                    default:
                    {
                        // illegal state
                        exit(7);
                    }
                }
                break;
            }
            default:
            {
                // illegal state
                exit(8);
            }
        }
    }
}

auto yielding_actor::dq(mailbox_element& node,
                        partial_function& fun) -> dq_result
{
    CPPA_REQUIRE(node.msg.cvals().get() != nullptr);
    if (node.marked) return dq_indeterminate;
    switch (filter_msg(node.msg))
    {
        case normal_exit_signal:
        case expired_timeout_message:
        {
            // skip message
            return dq_indeterminate;
        }
        case timeout_message:
        {
            // m_active_timeout_id is already invalid
            m_has_pending_timeout_request = false;
            return dq_timeout_occured;
        }
        default: break;
    }
    std::swap(m_last_dequeued, node.msg);
    std::swap(m_last_sender, node.sender);
    //m_last_dequeued = node.msg;
    //m_last_sender = node.sender;
    // make sure no timeout is handled incorrectly in a nested receive
    ++m_active_timeout_id;
    // lifetime scope of qguard
    {
        // make sure nested receives do not process this node again
        mailbox_element::guard qguard{&node};
        // try to invoke given function
        if (fun(m_last_dequeued))
        {
            // client erases node later (keep it marked until it's removed)
            qguard.release();
            // this members are only valid during invocation
            m_last_dequeued.reset();
            m_last_sender.reset();
            // we definitely don't have a pending timeout now
            m_has_pending_timeout_request = false;
            return dq_done;
        }
    }
    // no match, restore members
    --m_active_timeout_id;
    std::swap(m_last_dequeued, node.msg);
    std::swap(m_last_sender, node.sender);
    return dq_indeterminate;
}

} } // namespace cppa::detail

#else // ifdef CPPA_DISABLE_CONTEXT_SWITCHING

namespace { int keep_compiler_happy() { return 42; } }

#endif // ifdef CPPA_DISABLE_CONTEXT_SWITCHING
