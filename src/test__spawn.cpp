#include "cppa/test.hpp"
#include "cppa/actor.hpp"
//#include "cppa/spawn.hpp"

using cppa::actor;

template<typename F>
actor spawn(F)
{
	return actor();
}

void test__spawn()
{

	CPPA_TEST(test__spawn);

	actor a0 = spawn([](){
//		receive(on<int>() >> [](int i){
//			reply(i + 2);
//		});
	});

	a0.send(42);

	CPPA_CHECK(true);

}
