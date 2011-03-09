#include <iostream>
#include <functional>

#include "test.hpp"

#include "cppa/on.hpp"
#include "cppa/actor.hpp"
#include "cppa/spawn.hpp"
#include "cppa/detail/scheduler.hpp"

using std::cout;
using std::endl;

using namespace cppa;

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
