#include <map>
#include <set>
#include <memory>
#include <cctype>
#include <atomic>
#include <vector>
#include <string>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <iostream>
#include <iterator>
#include <algorithm>
#include <stdexcept>

#include "test.hpp"

#include "cppa/serializer.hpp"
#include "cppa/deserializer.hpp"
#include "cppa/uniform_type_info.hpp"

#include "cppa/util/disjunction.hpp"
#include "cppa/util/callable_trait.hpp"

using std::cout;
using std::endl;

using cppa::object;
using cppa::uniform_type_info;

namespace {

struct foo
{
	int value;
	explicit foo(int val = 0) : value(val) { }
};

bool operator==(const foo& lhs, const foo& rhs)
{
	return lhs.value == rhs.value;
}

} // namespace <anonymous>

namespace {

static bool unused1 =
		cppa::uniform_type_info::announce<foo>(
			[] (cppa::serializer& s, const foo& f) {
				s << f.value;
			},
			[] (cppa::deserializer& d, foo& f) {
				d >> f.value;
			},
			[] (const foo& f) -> std::string {
				std::ostringstream ostr;
				ostr << f.value;
				return ostr.str();
			},
			[] (const std::string& str) -> foo* {
				std::istringstream istr(str);
				int tmp;
				istr >> tmp;
				return new foo(tmp);
			}
		);
bool unused2 = false;// = cppa::uniform_type_info::announce(typeid(foo), new uti_impl<foo>);
bool unused3 = false;//= cppa::uniform_type_info::announce(typeid(foo), new uti_impl<foo>);
bool unused4 = false;//= cppa::uniform_type_info::announce(typeid(foo), new uti_impl<foo>);

} // namespace <anonymous>

std::size_t test__uniform_type()
{
	CPPA_TEST(test__uniform_type);

	{
		//bar.create_object();
		object obj1 = cppa::uniform_typeid<foo>()->create();
		object obj2(obj1);
		CPPA_CHECK_EQUAL(obj1, obj2);
	}

	{
		object wstr_obj1 = cppa::uniform_typeid<std::wstring>()->create();
		cppa::object_cast<std::wstring>(wstr_obj1) = L"hello wstring!";
		object wstr_obj2 = cppa::uniform_typeid<std::wstring>()->from_string("hello wstring!");
		CPPA_CHECK_EQUAL(wstr_obj1, wstr_obj2);
		// couldn't be converted to ASCII
		cppa::object_cast<std::wstring>(wstr_obj1) = L"hello wstring\x05D4";
		std::string narrowed = wstr_obj1.to_string();
		CPPA_CHECK_EQUAL(narrowed, "hello wstring?");
	}

	int successful_announces =   (unused1 ? 1 : 0)
							   + (unused2 ? 1 : 0)
							   + (unused3 ? 1 : 0)
							   + (unused4 ? 1 : 0);

	CPPA_CHECK_EQUAL(successful_announces, 1);

	// test foo_object implementation
/*
	obj_ptr o = cppa::uniform_typeid<foo>()->create();
	o->from_string("123");
	CPPA_CHECK_EQUAL(o->to_string(), "123");
	int val = reinterpret_cast<const foo*>(o->value())->value;
	CPPA_CHECK_EQUAL(val, 123);
*/

	// these types (and only those) are present if the uniform_type_info
	// implementation is correct
	std::set<std::string> expected =
	{
		"@_::foo",						// name of <anonymous namespace>::foo
		"@i8", "@i16", "@i32", "@i64",	// signed integer names
		"@u8", "@u16", "@u32", "@u64",	// unsigned integer names
		"@str", "@wstr",				// strings
		"float", "double",				// floating points
		"@0",							// cppa::util::void_type
		// default announced cppa types
		"cppa::any_type",
		"cppa::intrusive_ptr<cppa::actor>"
	};

	if (sizeof(double) != sizeof(long double))
	{
		// long double is only present if it's not an alias for double
		expected.insert("long double");
	}

	// holds the type names we see at runtime
	std::set<std::string> found;

	// fetch all available type names
	auto types = cppa::uniform_type_info::instances();
	for (cppa::uniform_type_info* tinfo : types)
	{
		found.insert(tinfo->name());
	}

	// compare the two sets
	CPPA_CHECK_EQUAL(expected.size(), found.size());

	if (expected.size() == found.size())
	{
		CPPA_CHECK((std::equal(found.begin(), found.end(), expected.begin())));
	}

	return CPPA_TEST_RESULT;
}
