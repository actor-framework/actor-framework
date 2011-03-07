#include <iostream>
#include <functional>

#include "cppa/on.hpp"
#include "cppa/test.hpp"
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
//	return actor();
}

void pong()
{
	receive(on<int>() >> [](int value) {
		reply((value * 20) + 2);
	});
}

std::size_t test__spawn()
{

	CPPA_TEST(test__spawn);

	{
		actor sl = spawn(pong);
		sl.send(23.f);
		sl.send(2);
		receive(on<int>() >> [&](int value) {
			CPPA_CHECK_EQUAL(value, 42);
		});
	}

	return CPPA_TEST_RESULT;

}
