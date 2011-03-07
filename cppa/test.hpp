#ifndef TEST_HPP
#define TEST_HPP

#include <cstddef>
#include <iostream>

#define CPPA_TEST(name)                                                        \
struct cppa_test_scope                                                         \
{                                                                              \
	std::size_t error_count;                                                   \
	cppa_test_scope() : error_count(0) { }                                     \
	~cppa_test_scope()                                                         \
	{                                                                          \
		std::cout << error_count << " error(s) detected" << std::endl;         \
	}                                                                          \
} cppa_ts;                                                                     \
std::size_t& error_count = cppa_ts.error_count

#define CPPA_TEST_RESULT cppa_ts.error_count

#define CPPA_CHECK(line_of_code)                                               \
if (!(line_of_code))                                                           \
{                                                                              \
	std::cerr << "ERROR in file " << __FILE__ << " on line " << __LINE__       \
			  << " => " << #line_of_code << std::endl;                         \
	++error_count;                                                             \
} ((void) 0)

#define CPPA_CHECK_EQUAL(lhs_loc, rhs_loc)                                     \
if ((lhs_loc) != (rhs_loc))                                                    \
{                                                                              \
	std::cerr << "ERROR in file " << __FILE__ << " on line " << __LINE__       \
	<< " => " << #lhs_loc << " != " << #rhs_loc << std::endl;                  \
	++error_count;                                                             \
} ((void) 0)

std::size_t test__type_list();
std::size_t test__a_matches_b();
std::size_t test__atom();
std::size_t test__tuple();
std::size_t test__spawn();
std::size_t test__intrusive_ptr();
std::size_t test__serialization();

void test__queue_performance();

#endif // TEST_HPP
