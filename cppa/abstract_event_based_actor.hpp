#ifndef EVENT_DRIVEN_ACTOR_HPP
#define EVENT_DRIVEN_ACTOR_HPP

#include <stack>
#include <memory>
#include <vector>

#include "cppa/pattern.hpp"
#include "cppa/invoke_rules.hpp"
#include "cppa/util/either.hpp"
#include "cppa/detail/scheduled_actor.hpp"

namespace cppa {

class abstract_event_based_actor : public detail::scheduled_actor
{

    typedef detail::scheduled_actor super;
    typedef super::queue_node queue_node;
    typedef super::queue_node_buffer queue_node_buffer;

 protected:

    struct stack_element
    {
        util::either<invoke_rules*, timed_invoke_rules*> m_ptr;
        bool m_ownership;
        inline stack_element(invoke_rules* ptr, bool take_ownership)
            : m_ptr(ptr), m_ownership(take_ownership)
        {
        }
        inline stack_element(invoke_rules&& from)
            : m_ptr(new invoke_rules(std::move(from))), m_ownership(true)
        {
        }
        inline stack_element(timed_invoke_rules* ptr, bool take_ownership)
            : m_ptr(ptr), m_ownership(take_ownership)
        {
        }
        inline stack_element(timed_invoke_rules&& from)
            : m_ptr(new timed_invoke_rules(std::move(from))), m_ownership(true)
        {
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
            return *(m_ptr.left());
        }
        inline timed_invoke_rules& right()
        {
            return *(m_ptr.right());
        }
    };

    queue_node_buffer m_buffer;
    std::stack<stack_element, std::vector<stack_element> > m_loop_stack;

 public:

    void dequeue(invoke_rules&) /*override*/;

    void dequeue(timed_invoke_rules&) /*override*/;

    void resume(util::fiber*, resume_callback* callback) /*override*/;

    virtual void init() = 0;

    virtual void on_exit();

    template<typename Scheduler>
    abstract_event_based_actor*
    attach_to_scheduler(void (*enqueue_fun)(Scheduler*, scheduled_actor*),
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

    // provoke compiler errors for usage of receive() and related functions

    template<typename... Args>
    void receive(Args&&...)
    {
        static_assert(sizeof...(Args) < 0,
                      "You shall not use receive in an event-based actor. "
                      "Use become()/unbecome() instead.");
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
