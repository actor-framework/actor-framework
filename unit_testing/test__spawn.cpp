#include <iostream>
#include <functional>

#include "test.hpp"

#include "cppa/on.hpp"
#include "cppa/cppa.hpp"
#include "cppa/actor.hpp"
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
		auto sl = spawn(pong);
		send(sl, 23.f);
		send(sl, 2);
		receive(on<int>() >> [&](int value) {
			CPPA_CHECK_EQUAL(value, 42);
		});
	}

	await_all_actors_done();

	return CPPA_TEST_RESULT;

}
