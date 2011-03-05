#include <list>
#include <cstddef>

#include "cppa/test.hpp"
#include "cppa/intrusive_ptr.hpp"

#include "cppa/detail/ref_counted_impl.hpp"

using namespace cppa;

namespace { int rc_instances = 0; }

struct test_rc : public cppa::detail::ref_counted_impl<std::size_t>
{

	test_rc() { ++rc_instances; }

	virtual ~test_rc() { --rc_instances; }

	test_rc* create() { return new test_rc; }

};

typedef intrusive_ptr<test_rc> test_ptr;

test_rc* get_test_rc()
{
	return new test_rc;
}

test_ptr get_test_ptr()
{
	return get_test_rc();
}

std::size_t test__intrusive_ptr()
{

	CPPA_TEST(test__intrusive_ptr);

	{
		test_ptr p(new test_rc);
		CPPA_CHECK_EQUAL(rc_instances, 1);
		CPPA_CHECK(p->unique());
	}
	CPPA_CHECK_EQUAL(rc_instances, 0);

	{
		test_ptr p;
		p = new test_rc;
		CPPA_CHECK_EQUAL(rc_instances, 1);
		CPPA_CHECK(p->unique());
	}
	CPPA_CHECK_EQUAL(rc_instances, 0);

	{
		test_ptr p1;
		p1 = get_test_rc();
		test_ptr p2 = p1;
		CPPA_CHECK_EQUAL(rc_instances, 1);
		CPPA_CHECK_EQUAL(p1->unique(), false);
	}
	CPPA_CHECK_EQUAL(rc_instances, 0);

	{
		std::list<test_ptr> pl;
		pl.push_back(get_test_ptr());
		pl.push_back(get_test_rc());
		pl.push_back(pl.front()->create());
		CPPA_CHECK(pl.front()->unique());
		CPPA_CHECK_EQUAL(rc_instances, 3);
	}
	CPPA_CHECK_EQUAL(rc_instances, 0);

	return CPPA_TEST_RESULT;

}
