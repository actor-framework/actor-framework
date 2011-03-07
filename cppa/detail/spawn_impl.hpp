#ifndef SPAWN_IMPL_HPP
#define SPAWN_IMPL_HPP

#include "cppa/actor.hpp"
#include "cppa/detail/scheduler.hpp"

namespace cppa { namespace detail {

struct behavior
{
	virtual void act() = 0;
	virtual void on_exit() = 0;
};

actor spawn_impl(behavior*);

} } // namespace cppa::detail

#endif // SPAWN_IMPL_HPP
