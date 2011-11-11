#include <string>

#include "test.hpp"

#include "cppa/on.hpp"
#include "cppa/atom.hpp"
#include "cppa/tuple.hpp"
#include "cppa/pattern.hpp"

using namespace cppa;

size_t test__pattern()
{
    CPPA_TEST(test__pattern);
    // some pattern objects to play with
    pattern<atom_value, int, std::string> p0{util::wrapped<atom_value>()};
    pattern<atom_value, int, std::string> p1(atom("FooBar"));
    pattern<atom_value, int, std::string> p2(atom("FooBar"), 42);
    pattern<atom_value, int, std::string> p3(atom("FooBar"), 42, "hello world");
    pattern<atom_value, anything, std::string> p4(atom("FooBar"), anything(), "hello world");
    pattern<atom_value, anything> p5(atom("FooBar"));
    pattern<anything> p6;
    pattern<atom_value, anything> p7;
    pattern<atom_value, anything, std::string> p8;
    // each one should accept x ...
    auto x = make_tuple(atom("FooBar"), 42, "hello world");
    CPPA_CHECK(p0(x));
    CPPA_CHECK(p1(x));
    CPPA_CHECK(p2(x));
    CPPA_CHECK(p3(x));
    CPPA_CHECK(p4(x));
    CPPA_CHECK(p5(x));
    CPPA_CHECK(p6(x));
    CPPA_CHECK(p7(x));
    CPPA_CHECK(p8(x));
    // ... but p2 and p3 should reject y
    auto y = make_tuple(atom("FooBar"), 24, "hello world");
    CPPA_CHECK_EQUAL(p0(y), true);
    CPPA_CHECK_EQUAL(p1(y), true);
    CPPA_CHECK_EQUAL(p2(y), false);
    CPPA_CHECK_EQUAL(p3(y), false);
    CPPA_CHECK_EQUAL(p4(y), true);
    CPPA_CHECK_EQUAL(p5(y), true);
    CPPA_CHECK_EQUAL(p6(y), true);
    CPPA_CHECK_EQUAL(p7(y), true);
    CPPA_CHECK_EQUAL(p8(y), true);
    // let's check some invoke rules
    constexpr size_t num_lambdas = 6;
    bool lambda_invoked[num_lambdas];
    auto reset_invoke_states = [&]()
    {
        for (size_t i = 0; i < num_lambdas; ++i)
        {
            lambda_invoked[i] = false;
        }
    };
    reset_invoke_states();
    auto patterns =
    (
        on<int, anything, int>() >> [&](int v1, int v2)
        {
            CPPA_CHECK_EQUAL(v1, 1);
            CPPA_CHECK_EQUAL(v2, 3);
            lambda_invoked[0] = true;
        },
        on<std::string>() >> [&](const std::string& str)
        {
            CPPA_CHECK_EQUAL(str, "hello foo");
            lambda_invoked[1] = true;
        },
        on("1", val<int>(), any_vals) >> [&](int value)
        {
            CPPA_CHECK_EQUAL(value, 2);
            lambda_invoked[2] = true;
        },
        on(1, val<std::string>(), any_vals) >> [&](const std::string& str)
        {
            CPPA_CHECK_EQUAL(str, "2");
            lambda_invoked[3] = true;
        },
        on<atom("Foo"), int>() >> [&](int value)
        {
            CPPA_CHECK_EQUAL(value, 1);
            lambda_invoked[4] = true;
        },
        on_param_match() >> [&](double v1, const float& v2)
        {
            CPPA_CHECK_EQUAL(v1, 1.0);
            CPPA_CHECK_EQUAL(v2, 2.0f);
            lambda_invoked[5] = true;
        }
    );
    // invokes lambda 0
    patterns(make_tuple(1, "2", 3));
    CPPA_CHECK(lambda_invoked[0]);
    reset_invoke_states();
    // invokes lambda 1
    patterns(make_tuple("hello foo"));
    CPPA_CHECK(lambda_invoked[1]);
    reset_invoke_states();
    // invokes lambda 2
    patterns(make_tuple("1", 2, 3));
    CPPA_CHECK(lambda_invoked[2]);
    reset_invoke_states();
    // invokes lambda 3
    patterns(make_tuple(1, "2", "3"));
    CPPA_CHECK(lambda_invoked[3]);
    reset_invoke_states();
    // invokes lambda 4
    patterns(make_tuple(atom("Foo"), 1));
    CPPA_CHECK(lambda_invoked[4]);
    reset_invoke_states();
    // invokes lambda 5
    patterns(make_tuple(1.0, 2.0f));
    CPPA_CHECK(lambda_invoked[5]);
    reset_invoke_states();

    return CPPA_TEST_RESULT;
}
