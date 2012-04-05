#include <functional>

#include "test.hpp"
#include "cppa/on.hpp"
#include "cppa/match.hpp"
#include "cppa/announce.hpp"
#include "cppa/to_string.hpp"

using namespace cppa;

using std::vector;
using std::string;

enum operator_id
{
    addition_op,
    subtraction_op,
    multiplication_op,
    division_op,
    modulo_op,
    less_op,
    less_eq_op,
    greater_op,
    greater_eq_op,
    equal_op,
    not_equal_op,
    exec_fun_op,
    logical_and_op,
    logical_or_op
};

#define CPPA_DISPATCH_OP(EnumValue, Operator)                                  \
    template<typename T1, typename T2>                                         \
    inline auto eval_op(std::integral_constant<operator_id, EnumValue >,       \
                        T1 const& lhs, T2 const& rhs) const                    \
    -> decltype(lhs Operator rhs) { return lhs Operator rhs; }

template<operator_id, typename First, typename Second>
struct guard_expr;

template<int X>
struct guard_placeholder
{

    constexpr guard_placeholder() { }

    template<typename Fun>
    guard_expr<exec_fun_op, guard_placeholder, Fun> operator()(Fun f) const
    {
        return {*this, std::move(f)};
    }

};

template<operator_id OP, typename First, typename Second>
struct guard_expr;

template<typename... Ts, typename T>
T const& fetch(detail::tdata<Ts...> const&, T const& value)
{
    return value;
}

template<typename... Ts, int X>
auto fetch(detail::tdata<Ts...> const& tup, guard_placeholder<X>)
     -> decltype(*get<X>(tup))
{
    return *get<X>(tup);
}

template<typename... Ts, operator_id OP, typename First, typename Second>
auto fetch(detail::tdata<Ts...> const& tup,
           guard_expr<OP, First, Second> const& ge)
     -> decltype(ge.eval(tup));

template<class Tuple, operator_id OP, typename First, typename Second>
auto eval_guard_expr(std::integral_constant<operator_id, OP> token,
                     Tuple const& tup,
                     First const& lhs,
                     Second const& rhs)
     -> decltype(eval_op(token, fetch(tup, lhs), fetch(tup, rhs)))
{
    return eval_op(token, fetch(tup, lhs), fetch(tup, rhs));
}

template<class Tuple, typename First, typename Second>
bool eval_guard_expr(std::integral_constant<operator_id, logical_and_op>,
                     Tuple const& tup,
                     First const& lhs,
                     Second const& rhs)
{
    // emulate short-circuit evaluation
    if (fetch(tup, lhs)) return fetch(tup, rhs);
    return false;
}

template<class Tuple, typename First, typename Second>
bool eval_guard_expr(std::integral_constant<operator_id, logical_or_op>,
                     Tuple const& tup,
                     First const& lhs,
                     Second const& rhs)
{
    // emulate short-circuit evaluation
    if (fetch(tup, lhs)) return true;
    return fetch(tup, rhs);
}

template<typename T, class Tuple>
struct compute_
{
    typedef T type;
};

template<int X, typename... Ts>
struct compute_<guard_placeholder<X>, detail::tdata<Ts...> >
{
    typedef typename std::remove_pointer<typename util::at<X, Ts...>::type>::type type;
};


template<operator_id OP, typename First, typename Second, class Tuple>
struct compute_result_type
{
    typedef bool type;
};

template<typename First, typename Second, class Tuple>
struct compute_result_type<addition_op, First, Second, Tuple>
{
    typedef typename compute_<First, Tuple>::type lhs_type;
    typedef typename compute_<Second, Tuple>::type rhs_type;
    typedef decltype(lhs_type() + rhs_type()) type;
};

template<operator_id OP, typename First, typename Second>
struct guard_expr
{
    std::pair<First, Second> m_args;
    template<typename F, typename S>
    guard_expr(F&& f, S&& s) : m_args(std::forward<F>(f), std::forward<S>(s)) { }
    guard_expr() = default;
    guard_expr(guard_expr&& other) : m_args(std::move(other.m_args)) { }
    guard_expr(guard_expr const&) = default;
    template<typename... Args>
    auto eval(detail::tdata<Args...> const& tup) const
         -> typename compute_result_type<OP, First, Second, detail::tdata<Args...>>::type
    {
        std::integral_constant<operator_id, OP> token;
        return eval_guard_expr(token, tup, m_args.first, m_args.second);
    }
    template<typename... Args>
    auto operator()(Args const&... args) const
         -> typename compute_result_type<OP, First, Second, detail::tdata<Args...>>::type
    {
        detail::tdata<Args const*...> tup{&args...};
        return eval(tup);
    }
};

template<typename... Ts, operator_id OP, typename First, typename Second>
auto fetch(detail::tdata<Ts...> const& tup,
           guard_expr<OP, First, Second> const& ge)
     -> decltype(ge.eval(tup))
{
    return ge.eval(tup);
}

static constexpr guard_placeholder<0> _x1;
static constexpr guard_placeholder<1> _x2;
static constexpr guard_placeholder<2> _x3;
static constexpr guard_placeholder<3> _x4;
static constexpr guard_placeholder<4> _x5;
static constexpr guard_placeholder<5> _x6;
static constexpr guard_placeholder<6> _x7;
static constexpr guard_placeholder<7> _x8;
static constexpr guard_placeholder<8> _x9;

#define GUARD_PLACEHOLDER_OPERATOR(EnumValue, Operator)                        \
    template<int Pos1, int Pos2>                                               \
    guard_expr< EnumValue , guard_placeholder<Pos1>, guard_placeholder<Pos2>>  \
    operator Operator (guard_placeholder<Pos1>, guard_placeholder<Pos2>)       \
    { return {}; }                                                             \
    template<int Pos, typename T>                                              \
    guard_expr< EnumValue , guard_placeholder<Pos>,                            \
                typename detail::strip_and_convert<T>::type >                  \
    operator Operator (guard_placeholder<Pos> gp, T value)                     \
    { return {std::move(gp), std::move(value)}; }                              \
    template<typename T, int Pos>                                              \
    guard_expr< EnumValue ,                                                    \
                typename detail::strip_and_convert<T>::type,                   \
                guard_placeholder<Pos> >                                       \
    operator Operator (T value, guard_placeholder<Pos> gp)                     \
    { return {std::move(value), std::move(gp)}; }

GUARD_PLACEHOLDER_OPERATOR(addition_op, +)
GUARD_PLACEHOLDER_OPERATOR(subtraction_op, -)
GUARD_PLACEHOLDER_OPERATOR(multiplication_op, *)
GUARD_PLACEHOLDER_OPERATOR(division_op, /)
GUARD_PLACEHOLDER_OPERATOR(modulo_op, %)
GUARD_PLACEHOLDER_OPERATOR(less_op, <)
GUARD_PLACEHOLDER_OPERATOR(less_eq_op, <=)
GUARD_PLACEHOLDER_OPERATOR(greater_op, >=)
GUARD_PLACEHOLDER_OPERATOR(greater_eq_op, >=)
GUARD_PLACEHOLDER_OPERATOR(equal_op, ==)
GUARD_PLACEHOLDER_OPERATOR(not_equal_op, !=)

template<operator_id OP1, typename F1, typename S1,
         operator_id OP2, typename F2, typename S2>
guard_expr<logical_and_op, guard_expr<OP1, F1, S1>, guard_expr<OP2, F2, S2>>
operator&&(guard_expr<OP1, F1, S1> lhs,
           guard_expr<OP2, F2, S2> rhs)
{
    return {std::move(lhs), std::move(rhs)};
}

template<operator_id OP1, typename F1, typename S1,
         operator_id OP2, typename F2, typename S2>
guard_expr<logical_or_op, guard_expr<OP1, F1, S1>, guard_expr<OP2, F2, S2>>
operator||(guard_expr<OP1, F1, S1> lhs,
           guard_expr<OP2, F2, S2> rhs)
{
    return {std::move(lhs), std::move(rhs)};
}

template<operator_id OP, typename F, typename S, typename T>
guard_expr<equal_op, guard_expr<OP, F, S>, T>
operator==(guard_expr<OP, F, S> lhs,
           T rhs)
{
    return {std::move(lhs), std::move(rhs)};
}

template<typename TupleTypes, class Fun>
bool eval_(any_tuple const& tup,
           Fun const& fun)
{
    typename util::tl_concat<TupleTypes, util::type_list<anything>>::type
        cast_token;
    auto x = tuple_cast(tup, cast_token);
    CPPA_REQUIRE(static_cast<bool>(x) == true);
    return util::unchecked_apply_tuple<bool>(fun, *x);
}

template<class GuardExpr, class TupleTypes>
struct guard_expr_value_matcher : value_matcher
{
    GuardExpr m_expr;
    guard_expr_value_matcher(GuardExpr&& ge) : m_expr(std::move(ge)) { }
    bool operator()(any_tuple const& tup) const
    {
        typename util::tl_concat<TupleTypes, util::type_list<anything>>::type
            cast_token;
        auto x = tuple_cast(tup, cast_token);
        CPPA_REQUIRE(static_cast<bool>(x) == true);
        return util::unchecked_apply_tuple<bool>(m_expr, *x);
    }
};


bool is_even(int i) { return i % 2 == 0; }

struct foobaz
{
    template<typename... Args>
    bool operator()(Args const&... args)
    {
        detail::tdata<Args const*...> tup{&args...};
        cout << "0: " << fetch(tup, _x1) << endl;
        return true;
    }
};

template<operator_id OP, typename First, typename Second>
value_matcher* when(guard_expr<OP, First, Second> ge)
{
    return nullptr;
}

std::string to_string(operator_id op)
{
    switch (op)
    {
        case addition_op:    return "+";
        case less_op:        return "<";
        case less_eq_op:     return "<=";
        case greater_op:     return ">";
        case greater_eq_op:  return ">=";
        case equal_op:       return "==";
        case not_equal_op:   return "!=";
        case logical_and_op: return "&&";
        case logical_or_op:  return "||";
        default:             return "???";
    }
}

/*
template<typename T>
std::string to_string(T const& value)
{
    std::ostringstream oss;
    oss << value;
    return oss.str();
}
*/

template<int X>
std::string to_string(guard_placeholder<X>)
{
    return "_x" + std::to_string(X+1);
}

template<typename... Ts, operator_id OP, typename First, typename Second>
std::string to_string(guard_expr<OP, First, Second> const& ge)
{
    std::string result;
    result += "(";
    result += to_string(ge.m_args.first);
    result += to_string(OP);
    result += to_string(ge.m_args.second);
    result += ")";
    return result;
}

/*
 *
 * projection:
 *
 * on(_x.rsplit("--(.*)=(.*)")) >> [](std::vector<std::string> matched_groups) { }
 *
 */

size_t test__match()
{
    CPPA_TEST(test__match);

    using namespace std::placeholders;

    foobaz fb;
    fb(1);

    typedef util::type_list<int, int, int, int> ttypes;
    any_tuple testee = make_cow_tuple(10, 20, 30, 40);
    cout << "testee[0] < testee[1] = "
         << eval_<ttypes>(testee, _x1 < _x2)
         << " ---- > "
         << eval_<ttypes>(testee, _x1 < _x2 && _x2 < 50 && _x3 < _x4 && _x4 == 40)
         << " ---- > "
         << eval_<ttypes>(testee, 5 < _x1)
         << "; is_even(10): "
         << eval_<ttypes>(testee, _x1(is_even))
         << "; _x1 + _x2 < _x3: "
         << eval_<ttypes>(testee, _x1 + _x2 < _x3)
         << endl;

    auto crazy_shit1 = _x1 < _x2 && _x2 < 50 && _x3 < _x4 && _x4 == 40;
    auto crazy_shit2 = _x1 < _x2 && (_x2 < 50 && _x3 < _x4 && _x4 == 40);

    cout << to_string(crazy_shit1) << endl;
    cout << to_string(crazy_shit2) << endl;

    auto expr1 = _x1 + _x2;
    auto expr2 = _x1 + _x2 < _x3;
    auto expr3 = _x1 % _x2 == 0;
    CPPA_CHECK_EQUAL(5, (expr1(2, 3)));
    CPPA_CHECK_EQUAL("12", (expr1(std::string{"1"}, std::string{"2"})));
    CPPA_CHECK_EQUAL("((_x1+_x2)<_x3)", to_string(expr2));
    CPPA_CHECK((expr3(100, 2)));

    auto expr4 = _x1 == "-h" || _x1 == "--help";
    CPPA_CHECK(expr4(std::string("-h")));
    CPPA_CHECK(expr4(std::string("--help")));
    CPPA_CHECK(!expr4(std::string("-g")));

    exit(0);

    /*
    auto xfun = _x.any_of<int>({1, 2, 3});
    cout << "xfun(4) = " << xfun(4) << endl;
    cout << "xfun(2) = " << xfun(2) << endl;

    auto lfun = _x < 5;
    cout << "lfun(4) = " << lfun(4) << endl;
    cout << "lfun(6) = " << lfun(6) << endl;

    cout << "sizeof(std::function<bool (int const&)>) = "
         << sizeof(std::function<bool (int const&)>)
         << endl;

    _on(_x.any_of({"-h", "--help"}), _x == 5);

    auto hfun = _x.any_of({"-h", "--help"});
    cout << "hfun(-h) = " << hfun("-h") << endl;
    cout << "hfun(-r) = " << hfun("-r") << endl;
    */

    bool invoked = false;
    match("abc")
    (
        on("abc") >> [&]()
        {
            invoked = true;
        }
    );
    if (!invoked) { CPPA_ERROR("match(\"abc\") failed"); }
    invoked = false;

    match_each({"-h", "-wtf"})
    (
        on(_x.any_of({"-h", "--help"})) >> []()
        {
            cout << "AWESOME!!!" << endl;
        },
        on(_x.starts_with("-")) >> [](std::string const& str)
        {
            match_each(str.begin() + 1, str.end())
            (
                on<char>() >> [](char c)
                //on(_x.any_of({'w', 't', 'f'})) >> [](char c)
                {
                    cout << c;
                },
                others() >> [](any_tuple const& tup)
                {
                    cout << "oops: " << to_string(tup) << endl;
                }
            );
            cout << endl;
        }
    );

    match(5)
    (
        on(_x < 6) >> [](int i)
        {
            cout << i << " < 6 !!! :)" << endl;
        }
    );

    vector<string> vec{"a", "b", "c"};
    match(vec)
    (
        on("a", "b", arg_match) >> [&](string& str)
        {
            invoked = true;
            str = "C";
        }
    );
    if (!invoked) { CPPA_ERROR("match({\"a\", \"b\", \"c\"}) failed"); }
    CPPA_CHECK_EQUAL("C", vec.back());
    invoked = false;

    match_each(vec)
    (
        on("a") >> [&](string& str)
        {
            invoked = true;
            str = "A";
        }
    );
    if (!invoked) { CPPA_ERROR("match_each({\"a\", \"b\", \"C\"}) failed"); }
    CPPA_CHECK_EQUAL("A", vec.front());
    invoked = false;

    match(vec)
    (
        others() >> [&](any_tuple& tup)
        {
            if (detail::matches<string, string, string>(tup))
            {
                tup.get_as_mutable<string>(1) = "B";
            }
            else
            {
                CPPA_ERROR("matches<string, string, string>(tup) == false");
            }
            invoked = true;
        }
    );
    if (!invoked) { CPPA_ERROR("match({\"a\", \"b\", \"c\"}) failed"); }
    CPPA_CHECK_EQUAL(vec[1], "B");
    invoked = false;

    vector<string> vec2{"a=0", "b=1", "c=2"};

    auto c2 = split(vec2.back(), '=');
    match(c2)
    (
        on("c", "2") >> [&]() { invoked = true; }
    );
    CPPA_CHECK_EQUAL(true, invoked);
    invoked = false;

    int pmatches = 0;
    using std::placeholders::_1;
    pmatch_each(vec2.begin(), vec2.end(), std::bind(split, _1, '='))
    (
        on("a", arg_match) >> [&](string const& value)
        {
            CPPA_CHECK_EQUAL("0", value);
            CPPA_CHECK_EQUAL(0, pmatches);
            ++pmatches;
        },
        on("b", arg_match) >> [&](string const& value)
        {
            CPPA_CHECK_EQUAL("1", value);
            CPPA_CHECK_EQUAL(1, pmatches);
            ++pmatches;
        },
        on("c", arg_match) >> [&](string const& value)
        {
            CPPA_CHECK_EQUAL("2", value);
            CPPA_CHECK_EQUAL(2, pmatches);
            ++pmatches;
        },
        others() >> [](any_tuple const& value)
        {
            cout << to_string(value) << endl;
        }
    );
    CPPA_CHECK_EQUAL(3, pmatches);

    return CPPA_TEST_RESULT;
}
