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
} cppa_ts;

#define CPPA_TEST_RESULT cppa_ts.error_count

#ifdef CPPA_VERBOSE_CHECK
#define CPPA_CHECK(line_of_code)                                               \
if (!(line_of_code))                                                           \
{                                                                              \
    std::cerr << "ERROR in file " << __FILE__ << " on line " << __LINE__       \
              << " => " << #line_of_code << std::endl;                         \
    ++cppa_ts.error_count;                                                     \
}                                                                              \
else                                                                           \
{                                                                              \
    std::cout << "line " << __LINE__ << " passed" << endl;                     \
}                                                                              \
((void) 0)
#else
#define CPPA_CHECK(line_of_code)                                               \
if (!(line_of_code))                                                           \
{                                                                              \
    std::cerr << "ERROR in file " << __FILE__ << " on line " << __LINE__       \
              << " => " << #line_of_code << std::endl;                         \
    ++cppa_ts.error_count;                                                     \
} ((void) 0)
#endif

#define CPPA_CHECK_EQUAL(lhs_loc, rhs_loc) CPPA_CHECK(((lhs_loc) == (rhs_loc)))

std::size_t test__uniform_type();
std::size_t test__type_list();
std::size_t test__a_matches_b();
std::size_t test__atom();
std::size_t test__tuple();
std::size_t test__spawn();
std::size_t test__intrusive_ptr();
std::size_t test__serialization();
std::size_t test__local_group();
std::size_t test__primitive_variant();

void test__queue_performance();

#endif // TEST_HPP
