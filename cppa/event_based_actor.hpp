#ifndef EVENT_BASED_ACTOR_HPP
#define EVENT_BASED_ACTOR_HPP

#include "cppa/detail/abstract_event_based_actor.hpp"

namespace cppa {

class event_based_actor : public detail::abstract_event_based_actor
{

    inline void become_impl(invoke_rules& rules)
    {
        become(std::move(rules));
    }

    inline void become_impl(timed_invoke_rules&& rules)
    {
        become(std::move(rules));
    }

    template<typename Head, typename... Tail>
    void become_impl(invoke_rules& rules, Head&& head, Tail&&... tail)
    {
        become_impl(rules.splice(std::forward<Head>(head)),
                    std::forward<Tail>(tail)...);
    }

 protected:

    void unbecome();

    void become(invoke_rules&& behavior);

    void become(timed_invoke_rules&& behavior);

    template<typename Head, typename... Tail>
    void become(invoke_rules&& rules, Head&& head, Tail&&... tail)
    {
        invoke_rules tmp(std::move(rules));
        become_impl(tmp.splice(std::forward<Head>(head)),
                    std::forward<Tail>(tail)...);
    }

 public:

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

#endif // EVENT_BASED_ACTOR_HPP
