#include <atomic>
#include <iostream>

#include "cppa/on.hpp"
#include "cppa/context.hpp"
#include "cppa/scheduler.hpp"
#include "cppa/to_string.hpp"

#include "cppa/detail/thread.hpp"
#include "cppa/detail/actor_count.hpp"
#include "cppa/detail/mock_scheduler.hpp"
#include "cppa/detail/thread_pool_scheduler.hpp"
#include "cppa/detail/converted_thread_context.hpp"

namespace {

typedef std::uint32_t ui32;

std::atomic<cppa::scheduler*> m_instance;

/*
struct static_cleanup_helper
{
    ~static_cleanup_helper()
    {
        auto i = m_instance.load();
        while (i)
        {
            if (m_instance.compare_exchange_weak(i, nullptr))
            {
                delete i;
                i = nullptr;
            }
        }
    }
}
s_cleanup_helper;
*/

struct exit_observer : cppa::attachable
{
    ~exit_observer()
    {
        cppa::detail::dec_actor_count();
    }
};

} // namespace <anonymous>

namespace cppa {

struct scheduler_helper
{

    typedef intrusive_ptr<detail::converted_thread_context> ptr_type;

    scheduler_helper() : m_worker(new detail::converted_thread_context)
    {
        // do NOT increase actor count; worker is "invisible"
        detail::thread(&scheduler_helper::time_emitter, m_worker).detach();
    }

    ~scheduler_helper()
    {
        m_worker->enqueue(message(m_worker, m_worker, atom(":_DIE")));
    }

    ptr_type m_worker;

 private:

    static void time_emitter(ptr_type m_self);

};

void scheduler_helper::time_emitter(scheduler_helper::ptr_type m_self)
{
    // setup & local variables
    set_self(m_self.get());
    auto& queue = m_self->m_mailbox.queue();
    typedef std::pair<cppa::actor_ptr, cppa::any_tuple> future_msg;
    std::multimap<decltype(detail::now()), future_msg> messages;
    decltype(queue.pop()) msg_ptr = nullptr;
    decltype(detail::now()) now;
    bool done = false;
    // message handling rules
    auto rules =
    (
        on<util::duration, any_type*>() >> [&](util::duration d)
        {
            any_tuple tup = msg_ptr->msg.content().tail(1);
            if (!tup.empty())
            {
                // calculate timeout
                auto timeout = detail::now();
                timeout += d;
                future_msg fmsg(msg_ptr->msg.sender(), tup);
                messages.insert(std::make_pair(std::move(timeout),
                                               std::move(fmsg)));
            }
        },
        on<atom(":_DIE")>() >> [&]()
        {
            done = true;
        }
    );
    // loop
    while (!done)
    {
        while (msg_ptr == nullptr)
        {
            if (messages.empty())
            {
                msg_ptr = queue.pop();
            }
            else
            {
                now = detail::now();
                // handle timeouts (send messages)
                auto it = messages.begin();
                while (it != messages.end() && (it->first) <= now)
                {
                    auto& whom = (it->second).first;
                    auto& what = (it->second).second;
                    whom->enqueue(message(whom, whom, what));
                    messages.erase(it);
                    it = messages.begin();
                }
                // wait for next message or next timeout
                if (it != messages.end())
                {
                    msg_ptr = queue.try_pop(it->first);
                }
            }
        }
        rules(msg_ptr->msg.content());
        delete msg_ptr;
        msg_ptr = nullptr;
    }
}

scheduler::scheduler() : m_helper(new scheduler_helper)
{
}

scheduler::~scheduler()
{
    delete m_helper;
}

channel* scheduler::future_send_helper()
{
    return m_helper->m_worker.get();
}

void scheduler::await_others_done()
{
    detail::actor_count_wait_until((unchecked_self() == nullptr) ? 0 : 1);
}

void scheduler::register_converted_context(context* what)
{
    if (what)
    {
        detail::inc_actor_count();
        what->attach(new exit_observer);
    }
}

attachable* scheduler::register_hidden_context()
{
    detail::inc_actor_count();
    return new exit_observer;
}

void scheduler::exit_context(context* ctx, std::uint32_t reason)
{
    ctx->quit(reason);
}

void set_scheduler(scheduler* sched)
{
    scheduler* s = nullptr;
    if (m_instance.compare_exchange_weak(s, sched) == false)
    {
        throw std::runtime_error("scheduler already set");
    }
}

scheduler* get_scheduler()
{
    scheduler* result = m_instance.load();
    while (result == nullptr)
    {
        scheduler* new_instance = new detail::thread_pool_scheduler;
        if (m_instance.compare_exchange_weak(result, new_instance))
        {
            result = new_instance;
        }
        else
        {
            delete new_instance;
        }
    }
    return result;
}

} // namespace cppa
