#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

#include "cppa/actor.hpp"
#include "cppa/context.hpp"
#include "cppa/actor_behavior.hpp"
#include "cppa/scheduling_hint.hpp"

namespace cppa { namespace detail {

struct scheduler
{

	static actor_ptr spawn(actor_behavior*, scheduling_hint);
	static context* get_context();
	static void await_all_done();

};

} } // namespace cppa::detail

#endif // SCHEDULER_HPP
