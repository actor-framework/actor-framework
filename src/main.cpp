#include <map>
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
fun_name ();                                                                   \
std::cout << std::endl

using std::cout;
using std::endl;

typedef std::map<std::string, std::pair<std::string, std::string>> str3_map;

int main()
{

	std::cout << std::boolalpha;

	RUN_TEST(test__a_matches_b);
	RUN_TEST(test__intrusive_ptr);
	RUN_TEST(test__spawn);
	RUN_TEST(test__tuple);
	RUN_TEST(test__type_list);
	RUN_TEST(test__serialization);
	RUN_TEST(test__atom);

	return 0;

}
