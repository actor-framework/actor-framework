#include <string>

#include "test.hpp"

#include "cppa/on.hpp"
#include "cppa/atom.hpp"
#include "cppa/tuple.hpp"
#include "cppa/pattern.hpp"
#include "cppa/announce.hpp"

#include "cppa/util/option.hpp"
#include "cppa/util/enable_if.hpp"
#include "cppa/util/disable_if.hpp"
#include "cppa/util/conjunction.hpp"
#include "cppa/util/is_primitive.hpp"

#include "cppa/detail/types_array.hpp"
#include "cppa/detail/decorated_tuple.hpp"

using namespace cppa;

template<typename Arr>
void plot(Arr const& arr)
{
    cout << "{ ";
    for (size_t i = 0; i < arr.size(); ++i)
    {
        if (i > 0) cout << ", ";
        cout << "arr[" << i << "] = " << ((arr[i]) ? arr[i]->name() : "anything");
    }
    cout << " }" << endl;
}

typedef std::pair<int,int> foobar;

static detail::types_array<int,anything,float> arr1;
static detail::types_array<int,anything,foobar> arr2;

template<typename... P>
auto tuple_cast(any_tuple const& tup, pattern<P...> const& p) -> util::option<typename tuple_from_type_list<typename pattern<P...>::filtered_types>::type>
{
    typedef typename pattern<P...>::filtered_types filtered_types;
    typedef typename tuple_from_type_list<filtered_types>::type tuple_type;
    util::option<tuple_type> result;
    typename pattern<P...>::mapping_vector mv;
    if (p(tup, &mv))
    {
        if (mv.size() == tup.size()) // perfect match
        {
            result = tuple_type::from(tup.vals());
        }
        else
        {
            result = tuple_type::from(new detail::decorated_tuple<filtered_types::size>(tup.vals(), mv));
        }
    }
    return std::move(result);
}

struct match_helper
{
    any_tuple const& what;
    match_helper(any_tuple const& w) : what(w) { }
    void operator()(invoke_rules&& rules)
    {
        rules(what);
    }
};

match_helper match(any_tuple const& x)
{

    return { x };
}

size_t test__pattern()
{

    //std::vector<int> ivec = {1, 2, 3};

    match(make_tuple(1,2,3))
    (
        on_arg_match >> [](int a, int b, int c)
        {
            cout << "MATCH: " << a << ", " << b << ", " << c << endl;
        }
    );

    pattern<int, anything, int> i3;
    any_tuple i3_any_tup = make_tuple(1, 2, 3);
    auto opt = tuple_cast(i3_any_tup, i3);
    if (opt.valid())
    {
        cout << "x = ( " << get<0>(*opt) << ", " << get<1>(*opt) << " )" << endl;
    }

    announce<foobar>(&foobar::first, &foobar::second);

    plot(arr1);
    plot(arr2);

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
        on("1", val<int>, any_vals) >> [&](int value)
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
        on_arg_match >> [&](double v1, const float& v2)
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
