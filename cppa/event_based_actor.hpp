#ifndef EVENT_BASED_ACTOR_HPP
#define EVENT_BASED_ACTOR_HPP

#include "cppa/event_based_actor_mixin.hpp"

namespace cppa {

class event_based_actor : public event_based_actor_mixin<event_based_actor>
{

    friend class event_based_actor_mixin<event_based_actor>;

    typedef abstract_event_based_actor::stack_element stack_element;

    void clear();
    void do_become(invoke_rules* behavior, bool has_ownership);
    void do_become(timed_invoke_rules* behavior, bool has_ownership);

};

} // namespace cppa

#endif // EVENT_BASED_ACTOR_HPP
