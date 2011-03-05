#include <string>
#include <typeinfo>
#include <iostream>

#include "cppa/test.hpp"
#include "cppa/util.hpp"

using std::cout;
using std::endl;

using namespace cppa;
using namespace cppa::util;

unsigned int MurmurHash2 ( const void * key, int len, unsigned int seed )
{
	static_assert(sizeof(int) == 4,
				  "MurmurHash2 requires sizeof(int) == 4");

	// 'm' and 'r' are mixing constants generated offline.
	// They're not really 'magic', they just happen to work well.

	const unsigned int m = 0x5bd1e995;
	const int r = 24;

	// Initialize the hash to a 'random' value

	unsigned int h = seed ^ len;

	// Mix 4 bytes at a time into the hash

	const unsigned char * data = (const unsigned char *)key;

	while(len >= 4)
	{
		unsigned int k = *(unsigned int *)data;

		k *= m;
		k ^= k >> r;
		k *= m;

		h *= m;
		h ^= k;

		data += 4;
		len -= 4;
	}

	// Handle the last few bytes of the input array

	switch(len)
	{
	case 3: h ^= data[2] << 16;
	case 2: h ^= data[1] << 8;
	case 1: h ^= data[0];
			h *= m;
	};

	// Do a few final mixes of the hash to ensure the last few
	// bytes are well-incorporated.

	h ^= h >> 13;
	h *= m;
	h ^= h >> 15;

	return h;
}

unsigned int hash_of(const char* what, int what_length)
{
	return MurmurHash2(what, what_length, 0x15091984);
}

unsigned int hash_of(const std::string& what)
{
	return MurmurHash2(what.c_str(), what.size(), 0x15091984);
}

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

std::size_t test__atom()
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
