#ifndef SCHEDULED_ACTOR_HPP
#define SCHEDULED_ACTOR_HPP

#include "cppa/context.hpp"
#include "cppa/util/fiber.hpp"
#include "cppa/actor_behavior.hpp"
#include "cppa/util/single_reader_queue.hpp"
#include "cppa/detail/abstract_actor.hpp"
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
    task_scheduler* m_scheduler;
    yielding_message_queue m_mailbox;

    static void run(void* _this);

 public:

    // creates an invalid actor
    scheduled_actor();

    scheduled_actor(actor_behavior* behavior, task_scheduler* sched);
    ~scheduled_actor();

    void quit(std::uint32_t reason);

    void enqueue_to_scheduler();

    message_queue& mailbox() /*override*/;

    inline std::atomic<int>& state() { return m_mailbox.m_state; }

};

} } // namespace cppa::detail

#endif // SCHEDULED_ACTOR_HPP
