#ifndef EVENT_BASED_ACTOR_MIXIN_HPP
#define EVENT_BASED_ACTOR_MIXIN_HPP

#include "cppa/behavior.hpp"
#include "cppa/abstract_event_based_actor.hpp"

namespace cppa {

template<typename Derived>
class event_based_actor_mixin : public abstract_event_based_actor
{

    typedef abstract_event_based_actor super;

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

    inline Derived* d_this() { return static_cast<Derived*>(this); }

 protected:

    inline void become(behavior* bhvr)
    {
        d_this()->do_become(bhvr);
    }

    inline void become(invoke_rules* behavior)
    {
        d_this()->do_become(behavior, false);
    }

    inline void become(timed_invoke_rules* behavior)
    {
        d_this()->do_become(behavior, false);
    }

    inline void become(invoke_rules&& behavior)
    {
        d_this()->do_become(new invoke_rules(std::move(behavior)), true);
    }

    inline void become(timed_invoke_rules&& behavior)
    {
        d_this()->do_become(new timed_invoke_rules(std::move(behavior)), true);
    }

    template<typename Head, typename... Tail>
    void become(invoke_rules&& rules, Head&& head, Tail&&... tail)
    {
        invoke_rules tmp(std::move(rules));
        become_impl(tmp.splice(std::forward<Head>(head)),
                    std::forward<Tail>(tail)...);
    }

};

} // namespace cppa

#endif // EVENT_BASED_ACTOR_MIXIN_HPP
