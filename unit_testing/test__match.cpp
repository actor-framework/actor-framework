#include <functional>

#include "test.hpp"
#include "cppa/on.hpp"
#include "cppa/match.hpp"
#include "cppa/announce.hpp"
#include "cppa/to_string.hpp"

using namespace cppa;

using std::vector;
using std::string;

template<typename T>
std::function<bool (T const&)> is_any_of(std::vector<T> vec)
{
    return [=](T const& value)
    {
        return std::any_of(vec.begin(), vec.end(),
                           [&](T const& v) { return value == v; });
    };
}

template<typename T>
std::function<bool (T const&)> is_none_of(std::vector<T> vec)
{
    return [=](T const& value)
    {
        return std::none_of(vec.begin(), vec.end(),
                           [&](T const& v) { return value == v; });
    };
}

struct placeholder
{
    constexpr placeholder() { }
    template<typename T>
    std::function<bool (T const&)> operator<(T const& value) const
    {
        return [=](T const& other) { return other < value; };
    }
    template<typename T>
    std::function<bool (T const&)> operator<=(T const& value) const
    {
        return [=](T const& other) { return other <= value; };
    }
    template<typename T>
    std::function<bool (T const&)> operator>(T const& value) const
    {
        return [=](T const& other) { return other > value; };
    }
    template<typename T>
    std::function<bool (T const&)> operator>=(T const& value) const
    {
        return [=](T const& other) { return other >= value; };
    }
    template<typename T>
    std::function<bool (T const&)> operator==(T const& value) const
    {
        return [=](T const& other) { return other == value; };
    }
    template<typename T>
    std::function<bool (T const&)> operator!=(T const& value) const
    {
        return [=](T const& other) { return other != value; };
    }
    template<typename T>
    std::function<bool (T const&)> any_of(std::vector<T> vec) const
    {
        return [=](T const& value)
        {
                 return std::any_of(vec.begin(), vec.end(),
                                    [&](T const& v) { return value == v; });
        };
    }
    template<typename T>
    std::function<bool (typename detail::implicit_conversions<T>::type const&)>
    any_of(std::initializer_list<T> list) const
    {
        typedef typename detail::implicit_conversions<T>::type ctype;
        vector<ctype> vec;
        for (auto& i : list) vec.emplace_back(i);
        //vec.insert(vec.begin(), list.begin(), list.end());
        return [vec](ctype const& value)
        {
            return std::any_of(vec.begin(), vec.end(),
                               [&](ctype const& v) { return value == v; });
        };
    }
    template<typename T>
    std::function<bool (T const&)> none_of(std::vector<T> vec) const
    {
        return [=](T const& value)
        {
            return std::none_of(vec.begin(), vec.end(),
                               [&](T const& v) { return value == v; });
        };
    }
};

static constexpr placeholder _x;

template<typename... Args>
void _on(Args&&...)
{
}

size_t test__match()
{
    CPPA_TEST(test__match);

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
