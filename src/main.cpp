#include <map>
#include <atomic>
#include <string>
#include <limits>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <typeinfo>
#include <iostream>

#include "cppa/test.hpp"
#include "cppa/tuple.hpp"
#include "cppa/config.hpp"
#include "cppa/uniform_type_info.hpp"

#ifdef CPPA_GCC
#include <cxxabi.h>
#endif

#define RUN_TEST(fun_name)                                                     \
std::cout << "run " << #fun_name << " ..." << std::endl;                       \
errors += fun_name ();                                                         \
std::cout << std::endl

using std::cout;
using std::endl;

int main()
{

	std::cout << std::boolalpha;

/*
	std::atomic<std::int32_t> a(42);
	std::int32_t b, c;
	b = 42;
	c = 10;

	cout << "a.compare_exchange_weak(b, c, std::memory_order_acquire) = "
		 << a.compare_exchange_weak(b, c, std::memory_order_acquire) << endl;

	cout << "a = "  << a << endl;
*/

	std::size_t errors = 0;

	RUN_TEST(test__a_matches_b);
	RUN_TEST(test__intrusive_ptr);
	RUN_TEST(test__spawn);
	RUN_TEST(test__tuple);
	RUN_TEST(test__type_list);
	RUN_TEST(test__serialization);
	RUN_TEST(test__atom);

	cout << endl
		 << "error(s) in all tests: " << errors
		 << endl;

	cout << endl << "run queue performance test ... " << endl;
	test__queue_performance();

	return 0;

}
