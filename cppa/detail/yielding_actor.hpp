#ifndef YIELDING_ACTOR_HPP
#define YIELDING_ACTOR_HPP

#include "cppa/pattern.hpp"
#include "cppa/detail/delegate.hpp"
#include "cppa/detail/scheduled_actor.hpp"
#include "cppa/detail/yield_interface.hpp"
#include "cppa/util/singly_linked_list.hpp"

namespace cppa { namespace detail {

class yielding_actor : public scheduled_actor
{

    typedef scheduled_actor super;
    typedef super::queue_node queue_node;
    typedef util::singly_linked_list<queue_node> queue_node_buffer;

    util::fiber m_fiber;
    actor_behavior* m_behavior;
    bool m_has_pending_timeout_request;
    std::uint32_t m_active_timeout_id;
    pattern<atom_value, std::uint32_t> m_pattern;

    static void run(void* _this);

    void yield_until_not_empty();

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

    dq_result dq(std::unique_ptr<queue_node>& node,
                 invoke_rules_base& rules,
                 queue_node_buffer& buffer);

 public:

    template<typename Scheduler>
    yielding_actor(actor_behavior* behavior,
                    void (*enqueue_fun)(Scheduler*, scheduled_actor*),
                    Scheduler* sched)
        : super(enqueue_fun, sched)
        , m_fiber(&yielding_actor::run, this)
        , m_behavior(behavior)
        , m_has_pending_timeout_request(false)
        , m_active_timeout_id(0)
    {
    }

    ~yielding_actor();

    void dequeue(invoke_rules& rules) /*override*/;

    void dequeue(timed_invoke_rules& rules) /*override*/;

    void resume(util::fiber* from, resume_callback* callback) /*override*/;

};

} } // namespace cppa::detail

#endif // YIELDING_ACTOR_HPP
