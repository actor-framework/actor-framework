#ifndef MOCK_SCHEDULER_HPP
#define MOCK_SCHEDULER_HPP

#include "cppa/scheduler.hpp"

namespace cppa { namespace detail {

class mock_scheduler : public scheduler
{

 public:

	actor_ptr spawn(actor_behavior* behavior, scheduling_hint hint);
	void await_all_done();

};

} } // namespace cppa::detail

#endif // MOCK_SCHEDULER_HPP
