#include <functional>

#include "test.hpp"
#include "cppa/on.hpp"
#include "cppa/match.hpp"
#include "cppa/announce.hpp"
#include "cppa/to_string.hpp"

using namespace cppa;

using std::vector;
using std::string;

#define CPPA_GUARD_IMPL(GuardOperator)                                         \
    struct impl : util::guard<T>                                               \
    {                                                                          \
        T m_val;                                                               \
        impl(T&& val) : m_val(std::move(val)) { }                              \
        bool operator()(T const& other) const { return other < m_val; }        \
    };  return std::unique_ptr<util::guard<T>>{new impl{std::move(value)}}

struct pattern_placeholder
{
    constexpr pattern_placeholder() { }

    template<typename T>
    std::unique_ptr<util::guard<T>> operator<(T value) const
    {
        CPPA_GUARD_IMPL(<);
    }

    template<typename T>
    std::unique_ptr<util::guard<T>> operator<=(T value) const
    {
        CPPA_GUARD_IMPL(<=);
    }

    template<typename T>
    std::unique_ptr<util::guard<T>> operator>(T value) const
    {
        CPPA_GUARD_IMPL(>);
    }

    template<typename T>
    std::unique_ptr<util::guard<T>> operator>=(T value) const
    {
        CPPA_GUARD_IMPL(>=);
    }

    template<typename T>
    T operator==(T value) const
    {
        return value;
    }

    template<typename T>
    std::unique_ptr<util::guard<T>> operator!=(T value) const
    {
        CPPA_GUARD_IMPL(!=);
    }

    template<typename T>
    std::unique_ptr<util::guard<T>> any_of(std::vector<T> vec) const
    {
cout << "guard<vector<" << detail::demangle(typeid(T).name()) << ">>" << endl;
        typedef std::vector<T> vector_type;
        struct impl : util::guard<T>
        {
            vector_type m_vec;
            impl(vector_type&& v) : m_vec(std::move(v)) { }
            bool operator()(T const& value) const
            {
                cout << "guard<vector<" << detail::demangle(typeid(T).name()) << ">>::operator()(" << value << ")" << endl;
                return std::any_of(m_vec.begin(), m_vec.end(),
                                   [&](T const& val) { return val == value; });
            }
        };
        return std::unique_ptr<util::guard<T>>{new impl{std::move(vec)}};
    }

    template<typename T>
    std::unique_ptr<util::guard<typename detail::strip_and_convert<T>::type>>
    any_of(std::initializer_list<T> list) const
    {
        typedef typename detail::strip_and_convert<T>::type sc_type;
        std::vector<sc_type> vec;
        vec.reserve(list.size());
        for (auto& i : list) vec.emplace_back(i);
        return this->any_of(std::move(vec));
    }

    template<typename T>
    std::unique_ptr<util::guard<typename detail::strip_and_convert<T>::type>>
    starts_with(T substr) const
    {
        typedef typename detail::strip_and_convert<T>::type str_type;
        struct impl : util::guard<str_type>
        {
            str_type m_str;
            impl(str_type str) : m_str(std::move(str)) { }
            bool operator()(str_type const& str) const
            {
                return    m_str.size() <= str.size()
                       && std::equal(m_str.begin(), m_str.end(), str.begin());
            }
        };
        return std::unique_ptr<util::guard<str_type>>{new impl{std::move(substr)}};
    }


};

static constexpr pattern_placeholder _x;

template<typename... Args>
void _on(Args&&...)
{
}

size_t test__match()
{
    CPPA_TEST(test__match);

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
