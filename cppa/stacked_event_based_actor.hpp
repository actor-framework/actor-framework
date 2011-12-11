#ifndef STACKED_EVENT_BASED_ACTOR_HPP
#define STACKED_EVENT_BASED_ACTOR_HPP

#include "cppa/event_based_actor_mixin.hpp"

namespace cppa {

class stacked_event_based_actor : public event_based_actor_mixin<stacked_event_based_actor>
{

    friend class event_based_actor_mixin<stacked_event_based_actor>;

    typedef abstract_event_based_actor::stack_element stack_element;

    void do_become(invoke_rules* behavior, bool has_ownership);
    void do_become(timed_invoke_rules* behavior, bool has_ownership);

 protected:

    void unbecome();

};

} // namespace cppa

#endif // STACKED_EVENT_BASED_ACTOR_HPP
