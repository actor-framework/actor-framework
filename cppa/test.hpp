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

#define CPPA_CHECK(line_of_code)                                               \
if (!(line_of_code))                                                           \
{                                                                              \
	std::cerr << "ERROR in file " << __FILE__ << " on line " << __LINE__       \
			  << " => " << #line_of_code << std::endl;                         \
	++error_count;                                                             \
} ((void) 0)

#define CPPA_CHECK_EQUAL(line_of_code, expected)                               \
if ((line_of_code) != expected)                                                \
{                                                                              \
	std::cerr << "ERROR in file " << __FILE__ << " on line " << __LINE__       \
			  << " => " << #line_of_code << " != " << expected << std::endl;   \
	++error_count;                                                             \
} ((void) 0)

void test__type_list();
void test__a_matches_b();
void test__atom();
void test__tuple();
void test__spawn();
void test__intrusive_ptr();
void test__serialization();

#endif // TEST_HPP
