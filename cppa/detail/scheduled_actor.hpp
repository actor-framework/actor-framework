#ifndef SCHEDULED_ACTOR_HPP
#define SCHEDULED_ACTOR_HPP

#include "cppa/context.hpp"
#include "cppa/util/fiber.hpp"
#include "cppa/actor_behavior.hpp"
#include "cppa/util/single_reader_queue.hpp"
#include "cppa/detail/delegate.hpp"
#include "cppa/detail/abstract_actor.hpp"
#include "cppa/detail/yield_interface.hpp"
#include "cppa/detail/yielding_message_queue.hpp"

namespace cppa { namespace detail {

class task_scheduler;

class scheduled_actor : public abstract_actor<context>
{

    typedef abstract_actor<context> super;

    friend class task_scheduler;
    friend class util::single_reader_queue<scheduled_actor>;

    // intrusive next pointer (single_reader_queue)
    scheduled_actor* next;

    util::fiber m_fiber;
    actor_behavior* m_behavior;
    yielding_message_queue m_mailbox;

    delegate m_enqueue_to_scheduler;

    static void run(void* _this);

 public:

    // creates an invalid actor
    scheduled_actor();

    template<typename Scheduler>
    scheduled_actor(actor_behavior* behavior,
                    void (*enqueue_fun)(Scheduler*, scheduled_actor*),
                    Scheduler* sched)
        : next(nullptr)
        , m_fiber(&scheduled_actor::run, this)
        , m_behavior(behavior)
        , m_mailbox(this)
        , m_enqueue_to_scheduler(enqueue_fun, sched, this)
    {
    }

    ~scheduled_actor();

    void quit(std::uint32_t reason);

    inline void enqueue_to_scheduler()
    {
        m_enqueue_to_scheduler();
    }

    message_queue& mailbox() /*override*/;

    inline std::atomic<int>& state() { return m_mailbox.m_state; }

    inline int compare_exchange_state(int expected, int new_value) volatile
    {
        int e = expected;
        do
        {
            if (m_mailbox.m_state.compare_exchange_weak(e, new_value))
            {
                return new_value;
            }
        }
        while (e == expected);
        return e;
    }

    // still_ready_handler returns true if execution should be continued;
    // otherwise false
    template<typename StillReadyHandler, typename DoneHandler>
    static void execute(scheduled_actor* what,
                        util::fiber& from,
                        StillReadyHandler&& still_ready_handler,
                        DoneHandler&& done_handler);

    template<typename DoneHandler>
    inline static void execute(scheduled_actor* what,
                               util::fiber& from,
                               DoneHandler&& done_handler)
    {
        execute(what, from,
                []() -> bool { return true; },
                std::forward<DoneHandler>(done_handler));
    }

};

template<typename StillReadyHandler, typename DoneHandler>
void scheduled_actor::execute(scheduled_actor* what,
                              util::fiber& from,
                              StillReadyHandler&& still_ready_handler,
                              DoneHandler&& done_handler)
{
    set_self(what);
    for (;;)
    {
        call(&(what->m_fiber), &from);
        switch (yielded_state())
        {
            case yield_state::done:
            case yield_state::killed:
            {
                done_handler();
                return;
            }
            case yield_state::ready:
            {
                if (still_ready_handler()) break;
                else return;
            }
            case yield_state::blocked:
            {
                switch (what->compare_exchange_state(about_to_block, blocked))
                {
                    case ready:
                    {
                        if (still_ready_handler()) break;
                        else return;
                    }
                    case blocked:
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
            }
            default:
            {
                // illegal state
                exit(7);
            }
        }
    }
}

} } // namespace cppa::detail

#endif // SCHEDULED_ACTOR_HPP
