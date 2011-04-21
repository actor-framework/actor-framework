#include <map>
#include <set>
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

#include "cppa/uniform_type_info.hpp"
#include "cppa/util/default_object_base.hpp"

using std::cout;
using std::endl;

using cppa::uniform_type_info;

namespace {

struct foo
{
	int value;
	foo(int val = 0) : value(val) { }
};

class foo_object : public cppa::util::default_object_base<foo>
{

	typedef default_object_base<foo> super;

 public:

	foo_object(const uniform_type_info* uti, const foo& val = foo())
		: super(uti, val) { }

	object* copy /*[[override]]*/ () const
	{
		return new foo_object(type(), m_value);
	}

	std::string to_string /*[[override]]*/ () const
	{
		std::ostringstream sstr;
		sstr << m_value.value;
		return sstr.str();
	}

	void from_string /*[[override]]*/ (const std::string& str)
	{
		int tmp;
		std::istringstream istr(str);
		istr >> tmp;
		m_value.value = tmp;
	}

	void deserialize(cppa::deserializer&) { }

	void serialize(cppa::serializer&) const { }

};

bool unused1 = cppa::uniform_type_info::announce<foo_object>(typeid(foo));
bool unused2 = cppa::uniform_type_info::announce<foo_object>(typeid(foo));
bool unused3 = cppa::uniform_type_info::announce<foo_object>(typeid(foo));
bool unused4 = cppa::uniform_type_info::announce<foo_object>(typeid(foo));

typedef cppa::intrusive_ptr<cppa::object> obj_ptr;

} // namespace <anonymous>

std::size_t test__uniform_type()
{
	CPPA_TEST(test__uniform_type);

	int successful_announces =   (unused1 ? 1 : 0)
							   + (unused2 ? 1 : 0)
							   + (unused3 ? 1 : 0)
							   + (unused4 ? 1 : 0);

	CPPA_CHECK_EQUAL(successful_announces, 1);

	// test foo_object implementation
	obj_ptr o = cppa::uniform_typeid<foo>()->create();
	o->from_string("123");
	CPPA_CHECK_EQUAL(o->to_string(), "123");
	int val = reinterpret_cast<const foo*>(o->value())->value;
	CPPA_CHECK_EQUAL(val, 123);

	// these types (and only those) are present if the uniform_type_info
	// implementation is correct
	std::set<std::string> expected =
	{
		"@_::foo",						// name of <anonymous namespace>::foo
		"@i8", "@i16", "@i32", "@i64",	// signed integer names
		"@u8", "@u16", "@u32", "@u64",	// unsigned integer names
		"@str", "@wstr",				// strings
		"float", "double",				// floating points
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
	auto types = cppa::uniform_type_info::get_all();
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
