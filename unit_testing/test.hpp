#ifndef TEST_HPP
#define TEST_HPP

#include <map>
#include <string>
#include <cstddef>
#include <iostream>

#define CPPA_TEST(name)                                                        \
struct cppa_test_scope                                                         \
{                                                                              \
    size_t error_count;                                                        \
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

#define CPPA_ERROR(err_msg)                                                    \
std::cerr << err_msg << std::endl;                                             \
++cppa_ts.error_count

#define CPPA_CHECK_EQUAL(lhs_loc, rhs_loc) CPPA_CHECK(((lhs_loc) == (rhs_loc)))
#define CPPA_CHECK_NOT_EQUAL(lhs_loc, rhs_loc) CPPA_CHECK(((lhs_loc) != (rhs_loc)))

size_t test__yield_interface();
size_t test__remote_actor(const char* app_path, bool is_client,
                          const std::map<std::string, std::string>& args);
size_t test__ripemd_160();
size_t test__uniform_type();
size_t test__type_list();
size_t test__atom();
size_t test__tuple();
size_t test__spawn();
size_t test__pattern();
size_t test__intrusive_ptr();
size_t test__serialization();
size_t test__local_group();
size_t test__primitive_variant();
size_t test__fixed_vector();

void test__queue_performance();

using std::cout;
using std::endl;
using std::cerr;

#endif // TEST_HPP
