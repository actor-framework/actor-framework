#ifndef MOCK_SCHEDULER_HPP
#define MOCK_SCHEDULER_HPP

#include "cppa/scheduler.hpp"

namespace cppa { namespace detail {

class mock_scheduler : public scheduler
{

 public:

    actor_ptr spawn(abstract_event_based_actor* what);

    actor_ptr spawn(scheduled_actor*, scheduling_hint);

    static actor_ptr spawn(scheduled_actor*);

};

} } // namespace cppa::detail

#endif // MOCK_SCHEDULER_HPP
