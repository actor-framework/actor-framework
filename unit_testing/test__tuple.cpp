#include <list>
#include <string>
#include <utility>
#include <iostream>
#include <typeinfo>
#include <functional>

#include "test.hpp"

#include "cppa/on.hpp"
#include "cppa/util.hpp"
#include "cppa/tuple.hpp"
#include "cppa/match.hpp"
#include "cppa/invoke.hpp"
#include "cppa/get_view.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/tuple_view.hpp"
#include "cppa/invoke_rules.hpp"
#include "cppa/intrusive_ptr.hpp"
#include "cppa/uniform_type_info.hpp"

#include "cppa/detail/invokable.hpp"
#include "cppa/detail/intermediate.hpp"

using std::cout;
using std::endl;

using namespace cppa;
using namespace cppa::util;

namespace {

bool function_called = false;

void fun(const std::string&)
{
    function_called = true;
//	CPPA_CHECK((s == "Hello World"));
}

} // namespace <anonymous>

size_t test__tuple()
{

    CPPA_TEST(test__tuple);

    // test of filter_type_list
    typedef filter_type_list<any_type,
                             util::type_list<any_type*, float,
                                             any_type*, int, any_type>>::type
            tv_float_int;

    // tuples under test
    tuple<int, float, int, std::string> t1(42, .2f, 2, "Hello World");
    tuple<std::string> t2("foo");
    auto t3 = make_tuple(42, .2f, 2, "Hello World", 0);
    tuple<int, float, int, std::string> t4(42, .2f, 2, "Hello World");
    tuple<int, std::string, int> t5(42, "foo", 24);
    tuple<int> t6;
    // untyped tuples under test
    any_tuple ut0 = t1;
    // tuple views under test
    auto tv0 = get_view<any_type*, float, any_type*, int, any_type>(t1);
    auto tv1 = get_view<any_type*, int, any_type*>(tv0);
    auto tv2 = get_view<int, any_type*, std::string>(t1);
    auto tv3 = get_view<any_type*, int, std::string>(t1);

//	any_tuple ut1 = tv0;

    CPPA_CHECK(get<0>(t6) == 0);
    CPPA_CHECK(get<0>(tv2) == get<0>(t1));
    CPPA_CHECK(get<1>(tv2) == get<3>(t1));

    CPPA_CHECK(get<1>(tv2) == "Hello World");

    {
        tuple<int> t1_sub1(42);
        tuple<int, float> t1_sub2(42, .2f);
        tuple<int, float, int> t1_sub3(42, .2f, 2);
        CPPA_CHECK((util::compare_first_elements(t1, t1_sub1)));
        CPPA_CHECK((match<int, any_type*>(t1)));
        CPPA_CHECK((util::compare_first_elements(t1, t1_sub2)));
        CPPA_CHECK((match<int, any_type*, float, any_type*>(t1)));
        CPPA_CHECK((util::compare_first_elements(t1, t1_sub3)));
        CPPA_CHECK((match<int, float, int, any_type>(t1)));
    }

    {
        std::vector<size_t> tv3_mappings;
        match<any_type*, int, std::string>(t1, &tv3_mappings);
        CPPA_CHECK((   tv3_mappings.size() == 2
                    && tv3_mappings[0] == 2
                    && tv3_mappings[1] == 3));
    }

    CPPA_CHECK(get<0>(tv3) == get<2>(t1));
    CPPA_CHECK(get<1>(tv3) == get<3>(t1));

    CPPA_CHECK(!(tv2 == tv3));

    {
        int* foo_int = new int(42);
        decltype(new int(*foo_int)) foo_int_2 = new int(*foo_int);
        CPPA_CHECK_EQUAL(*foo_int, *foo_int_2);
        delete foo_int_2;
        delete foo_int;
    }

    CPPA_CHECK((match<int, any_type*, std::string>(ut0)));

//	CPPA_CHECK(ut0 == t1);

    CPPA_CHECK(get<0>(tv0) == .2f);
    CPPA_CHECK(get<1>(tv0) == 2);

    CPPA_CHECK(get<0>(tv1) == 2);

    CPPA_CHECK((get<0>(tv1) == get<1>(tv0)));
    CPPA_CHECK((&(get<0>(tv1)) == &(get<1>(tv0))));

    // force detaching of tv1 from tv0 (and t1)
    get_ref<0>(tv1) = 20;

    CPPA_CHECK(get<1>(tv0) == 2);
    CPPA_CHECK(get<0>(tv1) == 20);
    CPPA_CHECK((&(get<0>(tv1)) != &(get<1>(tv0))));
    CPPA_CHECK((&(get<1>(t1)) == &(get<0>(tv0))));

    bool l1_invoked = false;
    bool l2_invoked = false;
    bool l3_invoked = false;

    auto reset_invoke_states = [&]() {
        l1_invoked = l2_invoked = l3_invoked = false;
    };

    auto l1 = [&](int v0, float v1, int v2, const std::string& v3) {
        l1_invoked = true;
        CPPA_CHECK((get<0>(t1) == v0));
        CPPA_CHECK((get<1>(t1) == v1));
        CPPA_CHECK((get<2>(t1) == v2));
        CPPA_CHECK((get<3>(t1) == v3));
    };

    auto l2 = [&](float v0, int v1) {
        l2_invoked = true;
        CPPA_CHECK((get<0>(tv0) == v0));
        CPPA_CHECK((get<1>(tv0) == v1));
    };

    auto l3 = [&](const std::string& v0) {
        l3_invoked = true;
        CPPA_CHECK((get<0>(t2) == v0));
    };

    invoke(l1, t1);
    CPPA_CHECK(l1_invoked);
    reset_invoke_states();

    invoke(l2, tv0);
    CPPA_CHECK(l2_invoked);
    reset_invoke_states();

    invoke(l3, t2);
    CPPA_CHECK(l3_invoked);
    reset_invoke_states();

    auto inv = (  on<any_type*, float, any_type*, int, any_type>() >> l2
                , on<int, float, int, std::string, any_type*>() >> l1
                , on<any_type*, int, std::string, any_type*>() >> l3);

    CPPA_CHECK(inv(t1));
    CPPA_CHECK(!l1_invoked && l2_invoked && !l3_invoked);
    reset_invoke_states();

    CPPA_CHECK(inv(t5));
    CPPA_CHECK(!l1_invoked && !l2_invoked && l3_invoked);
    reset_invoke_states();

    CPPA_CHECK(inv(t3));
    CPPA_CHECK(l1_invoked && !l2_invoked && !l3_invoked);
    reset_invoke_states();

    auto intmd = inv.get_intermediate(t1);
    CPPA_CHECK(intmd != (reinterpret_cast<detail::intermediate*>(0)));
    if (intmd) intmd->invoke();
    CPPA_CHECK(!l1_invoked && l2_invoked && !l3_invoked);
    reset_invoke_states();

    (on<any_type*, std::string, any_type*>() >> fun)(t2);
    CPPA_CHECK(function_called);
    reset_invoke_states();

    bool l4_invoked = false;

    auto l4 = [&]() {
        l4_invoked = true;
    };

    auto inv2 = (
        on(any_vals, .1f, any_vals, 2, val<>()) >> l4,
        //on<any_type*, float, any_type*, int, any_type>(.1f, 2) >> l4,
        on(any_vals, val<float>(), any_vals, 2, val<>()) >> l2
        //on<any_type*, float, any_type*, int, any_type>(any_val, 2) >> l2
    );

    CPPA_CHECK((match<any_type*, float, any_type*, int, any_type>(t1)));

    CPPA_CHECK(inv2(t1));
    CPPA_CHECK(!l4_invoked);
    CPPA_CHECK(l2_invoked);
    reset_invoke_states();

    {
        any_type* x = nullptr;
        CPPA_CHECK(x == val<>());
        CPPA_CHECK(val<>() == x);
        CPPA_CHECK(val<>() == 42);
        CPPA_CHECK(val<>() == 24);
    }

    // test detaching of tuples
    auto t1_copy = t1;

    CPPA_CHECK((&(get<0>(t1_copy)) == &(get<0>(t1))));
    get_ref<0>(t1_copy) = 24; // this detaches t4 from t1
    CPPA_CHECK((&(get<0>(t1_copy)) != &(get<0>(t1))));
    CPPA_CHECK(get<0>(t1_copy) != get<0>(t1));
    CPPA_CHECK(get<1>(t1_copy) == get<1>(t1));
    CPPA_CHECK(get<2>(t1_copy) == get<2>(t1));
    CPPA_CHECK(get<3>(t1_copy) == get<3>(t1));
    CPPA_CHECK(t1 == t4);

    return CPPA_TEST_RESULT;

}
