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

namespace {

// encodes ASCII characters to 6bit encoding
constexpr char encoding_table[256] =
{
/*         ..0 ..1 ..2 ..3 ..4 ..5 ..6 ..7 ..8 ..9 ..A ..B ..C ..D ..E ..F    */
/* 0.. */    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
/* 1.. */    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
/* 2.. */    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
/* 3.. */    1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11,  0,  0,  0,  0,  0,
/* 4.. */    0, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26,
/* 5.. */   27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37,  0,  0,  0,  0, 38,
/* 6.. */    0, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53,
/* 7.. */   54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64,  0,  0,  0,  0,  0
};

// decodes 6bit characters to ASCII
constexpr char decoding_table[] = " 0123456789:"
                                  "ABCDEFGHIJKLMNOPQRSTUVWXYZ_"
                                  "abcdefghijklmnopqrstuvwxyz";

} // namespace <anonymous>

// use 6 bit per character, stored in a 64 bit integer = max. 10 characters
template<char... Str>
struct get_atom_value;

template<char C0, char... Str>
struct get_atom_value<C0, Str...>
{
    static_assert(sizeof...(Str) <= 9,
                  "only 10 characters are allowed");

    static constexpr char mapped_c0 = encoding_table[C0 >= 0 ? static_cast<size_t>(C0) : 0];

    inline static constexpr std::uint64_t _(std::uint64_t interim)
    {
        static_assert(mapped_c0 != 0,
                      "only alphanumeric characters, '_' or ':' are allowed");
        return get_atom_value<Str...>::_((interim << 6) | mapped_c0);
    }
};

template<>
struct get_atom_value<>
{
    inline static constexpr std::uint64_t _(std::uint64_t result)
    {
        return result;
    }
};

constexpr std::uint64_t atom_val(const char* str, std::uint64_t interim = 0)
{
    return (str[0] != '\0') ? atom_val(str + 1, (interim << 6) | encoding_table[static_cast<size_t>(str[0])]) : interim;
}

class atom
{

 protected:

    std::uint64_t m_value;

    constexpr atom(std::uint64_t val) : m_value(val) { }

 public:

    constexpr atom(const atom& other) : m_value(other.m_value) { }

    constexpr atom(const char* str) : m_value(atom_val(str)) { }

    template<char... Str>
    static constexpr atom get()
    {
        return atom(get_atom_value<Str...>::_(0));
    }

    std::uint64_t value() const
    {
        return m_value;
    }

};

std::string to_string(const atom& a)
{
    std::string result;
    result.reserve(11);
    for (auto x = a.value(); x != 0; x >>= 6)
    {
        // decode 6 bit characters to ASCII
        result += decoding_table[static_cast<size_t>(x & 0x3F)];
    }
    // result is reversed, since we read from right-to-left
    return std::string(result.rbegin(), result.rend());
}

bool operator==(const atom& lhs, const atom& rhs)
{
    return lhs.value() == rhs.value();
}

bool operator!=(const atom& lhs, const atom& rhs)
{
    return !(lhs == rhs);
}

static constexpr atom s_a1 = "abc";
static constexpr atom s_a3 = atom::get<'a','b','c'>();

static constexpr std::uint64_t s_foo = atom_val("abc");

size_t test__atom()
{

    CPPA_TEST(test__atom);

    cout << "a    = " << get_atom_value<'a'>::_(0) << endl;
    cout << "ab   = " << get_atom_value<'a', 'b'>::_(0) << endl;
    cout << "abc  = " << get_atom_value<'a', 'b', 'c'>::_(0) << endl;
    cout << "abcd = " << get_atom_value<'a', 'b', 'c', 'd'>::_(0) << endl;
    cout << "__exit = " << get_atom_value<'_','_','e','x','i','t'>::_(0) << endl;
    cout << "cppa:exit = " << get_atom_value<'c','p','p','a',':','e','x','i','t'>::_(0) << endl;
    cout << "cppa::exit = " << get_atom_value<'c','p','p','a',':',':','e','x','i','t'>::_(0) << endl;

    atom a3 = atom::get<'a','b','c'>();
    cout << "to_string(a3) = " << to_string(a3) << endl;

    cout << "a3.value() = " << a3.value() << endl;
    cout << "s_a1.value() = " << s_a1.value() << endl;
    cout << "s_foo = " << s_foo << endl;

    return CPPA_TEST_RESULT;

}
