#include <atomic>
#include <iostream>

#include "cppa/on.hpp"
#include "cppa/anything.hpp"
#include "cppa/to_string.hpp"
#include "cppa/scheduler.hpp"
#include "cppa/local_actor.hpp"

#include "cppa/detail/thread.hpp"
#include "cppa/detail/actor_count.hpp"
#include "cppa/detail/mock_scheduler.hpp"
#include "cppa/detail/singleton_manager.hpp"
#include "cppa/detail/thread_pool_scheduler.hpp"
#include "cppa/detail/converted_thread_context.hpp"

namespace {

typedef std::uint32_t ui32;

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
    }

    void start()
    {
        m_thread = detail::thread(&scheduler_helper::time_emitter, m_worker);
    }

    void stop()
    {
        m_worker->enqueue(nullptr, make_tuple(atom(":_DIE")));
        m_thread.join();
    }

    ptr_type m_worker;
    detail::thread m_thread;

 private:

    static void time_emitter(ptr_type m_self);

};

void scheduler_helper::time_emitter(scheduler_helper::ptr_type m_self)
{
    // setup & local variables
    set_self(m_self.get());
    auto& queue = m_self->m_mailbox.queue();
    //typedef std::pair<cppa::actor_ptr, decltype(queue.pop())> future_msg;
    std::multimap<decltype(detail::now()), decltype(queue.pop())> messages;
    decltype(queue.pop()) msg_ptr = nullptr;
    decltype(detail::now()) now;
    bool done = false;
    // message handling rules
    auto rules =
    (
        on<util::duration,actor_ptr,anything>() >> [&](const util::duration& d,
                                                       const actor_ptr&)
//        on<util::duration,anything>() >> [&](const util::duration& d)
        {
            //any_tuple msg = msg_ptr->msg.tail();
            // calculate timeout
            auto timeout = detail::now();
            timeout += d;
            messages.insert(std::make_pair(std::move(timeout),
                                           std::move(msg_ptr)));
        },
        on<atom(":_DIE")>() >> [&]()
        {
            done = true;
        },
        others() >> []() { }
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
                    auto ptr = it->second;
                    auto whom = const_cast<actor_ptr*>(reinterpret_cast<const actor_ptr*>(ptr->msg.at(1)));
                    if (*whom)
                    {
                        auto msg = ptr->msg.tail(2);
                        (*whom)->enqueue(ptr->sender.get(), std::move(msg));
                    }
                    messages.erase(it);
                    it = messages.begin();
                    delete ptr;
                }
                // wait for next message or next timeout
                if (it != messages.end())
                {
                    msg_ptr = queue.try_pop(it->first);
                }
            }
        }
        rules(msg_ptr->msg);
        //delete msg_ptr;
        msg_ptr = nullptr;
    }
}

scheduler::scheduler() : m_helper(new scheduler_helper)
{
}

void scheduler::start()
{
    m_helper->start();
}

void scheduler::stop()
{
    m_helper->stop();
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

void scheduler::register_converted_context(local_actor* what)
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

void scheduler::exit_context(local_actor* ctx, std::uint32_t reason)
{
    ctx->quit(reason);
}

void set_scheduler(scheduler* sched)
{
    if (detail::singleton_manager::set_scheduler(sched) == false)
    {
        throw std::runtime_error("scheduler already set");
    }
}

scheduler* get_scheduler()
{
    scheduler* result = detail::singleton_manager::get_scheduler();
    if (result == nullptr)
    {
        result = new detail::thread_pool_scheduler;
        try
        {
            set_scheduler(result);
        }
        catch (std::runtime_error&)
        {
            delete result;
            return detail::singleton_manager::get_scheduler();
        }
    }
    return result;
}

} // namespace cppa
