#include <list>
#include <string>
#include <utility>
#include <iostream>
#include <typeinfo>
#include <functional>
#include <type_traits>

#include "test.hpp"

#include "cppa/on.hpp"
#include "cppa/cow_tuple.hpp"
#include "cppa/pattern.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/to_string.hpp"
#include "cppa/tuple_cast.hpp"
#include "cppa/intrusive_ptr.hpp"
#include "cppa/uniform_type_info.hpp"

#include "cppa/detail/invokable.hpp"
#include "cppa/detail/intermediate.hpp"

using std::cout;
using std::endl;

using namespace cppa;

template<class Expr, class Guard, typename Result, typename... Args>
class tpartial_function
{

 public:

    tpartial_function(Guard&& g, Expr&& e)
        : m_guard(std::move(g)), m_expr(std::move(e))
    {
    }

    tpartial_function(tpartial_function&&) = default;
    tpartial_function(tpartial_function const&) = default;

    bool defined_at(Args const&... args)
    {
        return m_guard(args...);
    }

    Result operator()(Args const&... args)
    {
        return m_expr(args...);
    }


 private:

    detail::tdata<Args...> m_args;
    Guard m_guard;
    Expr m_expr;

};

template<class Expr, class Guard, typename Result, class ArgTypes>
struct tpf_;

template<class Expr, class Guard, typename Result, typename... Ts>
struct tpf_<Expr, Guard, Result, util::type_list<Ts...> >
{
    typedef tpartial_function<Expr, Guard, Result, Ts...> type;
};

template<class Expr, class Guard>

typename tpf_<Expr, Guard,
              typename util::get_callable_trait<Expr>::result_type,
              typename util::get_callable_trait<Expr>::arg_types
>::type
tfun(Expr e, Guard g)
{
    return {std::move(g), std::move(e)};
}

size_t test__tuple()
{
    CPPA_TEST(test__tuple);

    using namespace cppa::placeholders;

    // check type correctness of make_cow_tuple()
    auto t0 = make_cow_tuple("1", 2);
    CPPA_CHECK((std::is_same<decltype(t0), cppa::cow_tuple<std::string, int>>::value));
    auto t0_0 = get<0>(t0);
    auto t0_1 = get<1>(t0);
    // check implicit type conversion
    CPPA_CHECK((std::is_same<decltype(t0_0), std::string>::value));
    CPPA_CHECK((std::is_same<decltype(t0_1), int>::value));
    CPPA_CHECK_EQUAL(t0_0, "1");
    CPPA_CHECK_EQUAL(t0_1, 2);
    // use tuple cast to get a subtuple
    any_tuple at0(t0);
    auto v0opt = tuple_cast<std::string, anything>(at0);
    CPPA_CHECK((std::is_same<decltype(v0opt), option<cow_tuple<std::string>>>::value));
    CPPA_CHECK((v0opt));
    CPPA_CHECK(   at0.size() == 2
               && at0.at(0) == &get<0>(t0)
               && at0.at(1) == &get<1>(t0));
    if (v0opt)
    {
        auto& v0 = *v0opt;
        CPPA_CHECK((std::is_same<decltype(v0), cow_tuple<std::string>&>::value));
        CPPA_CHECK((std::is_same<decltype(get<0>(v0)), std::string const&>::value));
        CPPA_CHECK_EQUAL(v0.size(), 1);
        CPPA_CHECK_EQUAL(get<0>(v0), "1");
        CPPA_CHECK_EQUAL(get<0>(t0), get<0>(v0));
        // check cow semantics
        CPPA_CHECK_EQUAL(&get<0>(t0), &get<0>(v0));     // point to the same
        get_ref<0>(t0) = "hello world";                 // detaches t0 from v0
        CPPA_CHECK_EQUAL(get<0>(t0), "hello world");    // t0 contains new value
        CPPA_CHECK_EQUAL(get<0>(v0), "1");              // v0 contains old value
        CPPA_CHECK_NOT_EQUAL(&get<0>(t0), &get<0>(v0)); // no longer the same
        // check operator==
        auto lhs = make_cow_tuple(1,2,3,4);
        auto rhs = make_cow_tuple(static_cast<std::uint8_t>(1), 2.0, 3, 4);
        CPPA_CHECK(lhs == rhs);
        CPPA_CHECK(rhs == lhs);
    }
    any_tuple at1 = make_cow_tuple("one", 2, 3.f, 4.0);
    {
        // perfect match
        auto opt0 = tuple_cast<std::string, int, float, double>(at1);
        CPPA_CHECK(opt0);
        if (opt0)
        {
            CPPA_CHECK((*opt0 == make_cow_tuple("one", 2, 3.f, 4.0)));
            CPPA_CHECK_EQUAL(&get<0>(*opt0), at1.at(0));
            CPPA_CHECK_EQUAL(&get<1>(*opt0), at1.at(1));
            CPPA_CHECK_EQUAL(&get<2>(*opt0), at1.at(2));
            CPPA_CHECK_EQUAL(&get<3>(*opt0), at1.at(3));
        }
        // leading wildcard
        auto opt1 = tuple_cast<anything, double>(at1);
        CPPA_CHECK(opt1);
        if (opt1)
        {
            CPPA_CHECK_EQUAL(get<0>(*opt1), 4.0);
            CPPA_CHECK_EQUAL(&get<0>(*opt1), at1.at(3));
        }
        // trailing wildcard
        auto opt2 = tuple_cast<std::string, anything>(at1);
        CPPA_CHECK(opt2);
        if (opt2)
        {
            CPPA_CHECK_EQUAL(get<0>(*opt2), "one");
            CPPA_CHECK_EQUAL(&get<0>(*opt2), at1.at(0));
        }
        // wildcard in between
        auto opt3 = tuple_cast<std::string, anything, double>(at1);
        CPPA_CHECK(opt3);
        if (opt3)
        {
            CPPA_CHECK((*opt3 == make_cow_tuple("one", 4.0)));
            CPPA_CHECK_EQUAL(get<0>(*opt3), "one");
            CPPA_CHECK_EQUAL(get<1>(*opt3), 4.0);
            CPPA_CHECK_EQUAL(&get<0>(*opt3), at1.at(0));
            CPPA_CHECK_EQUAL(&get<1>(*opt3), at1.at(3));
        }
    }
    return CPPA_TEST_RESULT;
}
