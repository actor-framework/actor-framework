#include <string>
#include <typeinfo>
#include <iostream>

#include "test.hpp"
#include "hash_of.hpp"
#include "cppa/util.hpp"

using std::cout;
using std::endl;

using namespace cppa;
using namespace cppa::util;

template<char... Str>
struct _tostring;

template<char C0, char... Str>
struct _tostring<C0, Str...>
{
    inline static void _(std::string& s)
    {
        s += C0;
        _tostring<Str...>::_(s);
    }
};

template<>
struct _tostring<>
{
    inline static void _(std::string&) { }
};

class atom_base
{

    std::string m_str;

    unsigned int m_hash;

 public:

    atom_base(std::string&& str) : m_str(str), m_hash(hash_of(m_str))
    {
    }

    atom_base(atom_base&& rval)
        : m_str(std::move(rval.m_str)), m_hash(hash_of(m_str))
    {
    }

    unsigned int hash() const
    {
        return m_hash;
    }

    const std::string& value() const
    {
        return m_str;
    }

};

bool operator==(const atom_base& lhs, const atom_base& rhs)
{
    return (lhs.hash() == rhs.hash()) && (lhs.value() == rhs.value());
}

bool operator!=(const atom_base& lhs, const atom_base& rhs)
{
    return !(lhs == rhs);
}

bool operator!=(const atom_base& lhs, const std::string& rhs)
{
    return lhs.value() == rhs;
}

bool operator!=(const std::string& lhs, const atom_base& rhs)
{
    return lhs == rhs.value();
}

// template<char...>
// atom<Str...> operator "" _atom();

template<char... Str>
class atom : public atom_base
{
    static std::string to_string()
    {
        std::string result;
        _tostring<Str...>::_(result);
        return result;
    }

 public:

    atom() : atom_base(to_string()) { }
    atom(atom&& rval) : atom_base(rval) { }

};

size_t test__atom()
{

    CPPA_TEST(test__atom);

    atom<'f','o','o'>  a1;
    atom_base a2("foo");
    atom_base a3 = atom<'a','b','c'>();

    CPPA_CHECK(a1 == a2);
    CPPA_CHECK(a1 != a3);

//	atom<"foobar"> a1;

//	CPPA_CHECK(a1 == a3);

    return CPPA_TEST_RESULT;

}
