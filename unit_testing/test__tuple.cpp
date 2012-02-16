#include <list>
#include <string>
#include <utility>
#include <iostream>
#include <typeinfo>
#include <functional>
#include <type_traits>

#include "test.hpp"

#include "cppa/on.hpp"
#include "cppa/util.hpp"
#include "cppa/tuple.hpp"
#include "cppa/pattern.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/to_string.hpp"
#include "cppa/tuple_cast.hpp"
#include "cppa/invoke_rules.hpp"
#include "cppa/intrusive_ptr.hpp"
#include "cppa/uniform_type_info.hpp"

#include "cppa/detail/invokable.hpp"
#include "cppa/detail/intermediate.hpp"

using std::cout;
using std::endl;

using namespace cppa;

size_t test__tuple()
{
    CPPA_TEST(test__tuple);
    // check type correctness of make_tuple()
    auto t0 = make_tuple("1", 2);
    auto t0_0 = get<0>(t0);
    auto t0_1 = get<1>(t0);
    // check implicit type conversion
    CPPA_CHECK((std::is_same<decltype(t0_0), std::string>::value));
    CPPA_CHECK((std::is_same<decltype(t0_1), int>::value));
    CPPA_CHECK_EQUAL(t0_0, "1");
    CPPA_CHECK_EQUAL(t0_1, 2);
    // get a view of t0
    any_tuple atup0(t0);
    CPPA_CHECK(atup0.size() == 2 && atup0.at(0) == &get<0>(t0));
    /*
    auto v1opt = tuple_cast<std::string, anything>(any_tuple_view(atup0));
    // the tuple_view forces atup0 to detach from t0
    CPPA_CHECK(atup0.size() == 2 && atup0.at(0) != &get<0>(t0));
    CPPA_CHECK((v1opt));
    if (v1opt)
    {
        auto& v1 = *v1opt;
        CPPA_CHECK((std::is_same<decltype(v1), tuple_view<std::string>&>::value));
        CPPA_CHECK_EQUAL(v1.size(), 1);
        auto& v1_0 = get<0>(v1);
        CPPA_CHECK_EQUAL(v1_0, "1");
        CPPA_CHECK_EQUAL(atup0.at(0), &(get<0>(v1)));     // point to the same
    }
    */
    // use tuple cast to get a subtuple
    any_tuple at0(t0);
    auto v0opt = tuple_cast<std::string, anything>(at0);
    CPPA_CHECK((std::is_same<decltype(v0opt), option<tuple<std::string>>>::value));
    CPPA_CHECK((v0opt));
    CPPA_CHECK(at0.size() == 2 && at0.at(0) == &get<0>(t0));
    if (v0opt)
    {
        auto& v0 = *v0opt;
        CPPA_CHECK((std::is_same<decltype(v0), tuple<std::string>&>::value));
        auto& v0_0 = get<0>(v0);
        CPPA_CHECK((std::is_same<decltype(v0_0), std::string const&>::value));
        CPPA_CHECK_EQUAL(v0.size(), 1);
        CPPA_CHECK_EQUAL(v0_0, "1");
        CPPA_CHECK_EQUAL(get<0>(t0), get<0>(v0));
        // check cow semantics
        CPPA_CHECK_EQUAL(&get<0>(t0), &get<0>(v0));     // point to the same
        get_ref<0>(t0) = "hello world";                 // detaches t0 from v0
        CPPA_CHECK_EQUAL(get<0>(t0), "hello world");    // t0 contains new value
        CPPA_CHECK_EQUAL(get<0>(v0), "1");              // v0 contains old value
        CPPA_CHECK_NOT_EQUAL(&get<0>(t0), &get<0>(v0)); // no longer the same
        // check operator==
        auto lhs = make_tuple(1,2,3,4);
        auto rhs = make_tuple(static_cast<std::uint8_t>(1), 2.0, 3, 4);
        CPPA_CHECK_EQUAL(lhs, rhs);
        CPPA_CHECK_EQUAL(rhs, lhs);
    }
    return CPPA_TEST_RESULT;
}
