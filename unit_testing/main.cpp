#include <map>
#include <atomic>
#include <string>
#include <limits>
#include <vector>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <typeinfo>
#include <iostream>

#include "test.hpp"

#include "cppa/tuple.hpp"
#include "cppa/config.hpp"
#include "cppa/uniform_type_info.hpp"

#define RUN_TEST(fun_name)                                                     \
std::cout << "run " << #fun_name << " ..." << std::endl;                       \
errors += fun_name ();                                                         \
std::cout << std::endl

using std::cout;
using std::cerr;
using std::endl;

int main(int argc, char** c_argv)
{
	std::vector<std::string> argv;
	for (int i = 1; i < argc; ++i)
	{
		argv.push_back(c_argv[i]);
	}

	if (!argv.empty())
	{
		if (argv.size() == 1 && argv.front() == "performance_test")
		{
			cout << endl << "run queue performance test ... " << endl;
			test__queue_performance();
		}
		else
		{
			cerr << "unrecognized options"
				 << endl
				 << "no options:\n\tunit tests"
				 << endl
				 << "performance_test:\n\trun single reader queue tests"
				 << endl;
		}
	}
	else
	{
		std::cout << std::boolalpha;
		std::size_t errors = 0;
		RUN_TEST(test__a_matches_b);
		RUN_TEST(test__intrusive_ptr);
		RUN_TEST(test__spawn);
		RUN_TEST(test__tuple);
		RUN_TEST(test__type_list);
		RUN_TEST(test__serialization);
		RUN_TEST(test__atom);
		RUN_TEST(test__local_group);
		cout << endl
			 << "error(s) in all tests: " << errors
			 << endl;
	}
	return 0;
}
