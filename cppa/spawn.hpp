#ifndef SPAWN_HPP
#define SPAWN_HPP

#include <functional>

#include "cppa/actor.hpp"
#include "cppa/detail/scheduler.hpp"

using std::cout;
using std::endl;

using namespace cppa;

template<typename F>
actor spawn(F act_fun)
{
	struct bhv : cppa::detail::behavior
	{
		std::function<void ()> m_act;
		bhv(const F& invokable) : m_act(invokable) { }
		virtual void act()
		{
			m_act();
		}
		virtual void on_exit()
		{
		}
	};
	return cppa::detail::spawn_impl(new bhv(act_fun));
}

#endif // SPAWN_HPP
