#ifndef YIELDING_ACTOR_HPP
#define YIELDING_ACTOR_HPP

#include <stack>

#include "cppa/pattern.hpp"

#include "cppa/detail/delegate.hpp"
#include "cppa/detail/abstract_scheduled_actor.hpp"
#include "cppa/detail/yield_interface.hpp"

#include "cppa/util/either.hpp"
#include "cppa/util/singly_linked_list.hpp"

namespace cppa { namespace detail {

class yielding_actor : public abstract_scheduled_actor
{

    typedef abstract_scheduled_actor super;
    typedef super::queue_node queue_node;
    typedef util::singly_linked_list<queue_node> queue_node_buffer;

    util::fiber m_fiber;
    scheduled_actor* m_behavior;

    static void run(void* _this);

    void exec_loop_stack();

    void yield_until_not_empty();

 public:

    template<typename Scheduler>
    yielding_actor(scheduled_actor* behavior,
                    void (*enqueue_fun)(Scheduler*, abstract_scheduled_actor*),
                    Scheduler* sched)
        : super(enqueue_fun, sched)
        , m_fiber(&yielding_actor::run, this)
        , m_behavior(behavior)
    {
    }

    ~yielding_actor() /*override*/;

    void dequeue(invoke_rules& rules) /*override*/;

    void dequeue(timed_invoke_rules& rules) /*override*/;

    void resume(util::fiber* from, resume_callback* callback) /*override*/;

};

} } // namespace cppa::detail

#endif // YIELDING_ACTOR_HPP
