#ifndef EVENT_DRIVEN_ACTOR_HPP
#define EVENT_DRIVEN_ACTOR_HPP

#include <stack>
#include <memory>

#include "cppa/pattern.hpp"
#include "cppa/invoke_rules.hpp"
#include "cppa/util/either.hpp"
#include "cppa/detail/scheduled_actor.hpp"

namespace cppa { namespace detail {

class abstract_event_based_actor : public scheduled_actor
{

    typedef scheduled_actor super;
    typedef super::queue_node queue_node;
    typedef super::queue_node_buffer queue_node_buffer;

 protected:

    queue_node_buffer m_buffer;
    std::stack< util::either<invoke_rules, timed_invoke_rules> > m_loop_stack;

 public:

    void dequeue(invoke_rules&) /*override*/;

    void dequeue(timed_invoke_rules&) /*override*/;

    void resume(util::fiber*, resume_callback* callback) /*override*/;

 private:

    void handle_message(std::unique_ptr<queue_node>& node,
                        invoke_rules& behavior);

    void handle_message(std::unique_ptr<queue_node>& node,
                        timed_invoke_rules& behavior);

    void handle_message(std::unique_ptr<queue_node>& node);

};

} } // namespace cppa::detail

#endif // EVENT_DRIVEN_ACTOR_HPP
