#ifndef FSM_ACTOR_HPP
#define FSM_ACTOR_HPP

#include "cppa/event_based_actor.hpp"

namespace cppa {

template<class Derived>
class fsm_actor : public event_based_actor
{

 public:

    void init()
    {
        become(&(static_cast<Derived*>(this)->init_state));
    }

};

} // namespace cppa

#endif // FSM_ACTOR_HPP
