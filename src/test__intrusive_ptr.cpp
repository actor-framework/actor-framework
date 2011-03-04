#include <list>
#include <cstddef>
#include "cppa/test.hpp"
#include "cppa/intrusive_ptr.hpp"

using namespace cppa;

namespace { int rc_instances = 0; }

class test_rc
{

	std::size_t m_rc;

 public:

	test_rc() : m_rc(0) { ++rc_instances; }

	void ref() { ++m_rc; }

	bool deref() { return --m_rc > 0; }

	std::size_t rc() const { return m_rc; }

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

void test__intrusive_ptr()
{

	CPPA_TEST(test__intrusive_ptr);

	{
		test_ptr p(new test_rc);
		CPPA_CHECK_EQUAL(rc_instances, 1);
		CPPA_CHECK_EQUAL(p->rc(), 1);
	}
	CPPA_CHECK_EQUAL(rc_instances, 0);

	{
		test_ptr p;
		p = new test_rc;
		CPPA_CHECK_EQUAL(rc_instances, 1);
		CPPA_CHECK_EQUAL(p->rc(), 1);
	}
	CPPA_CHECK_EQUAL(rc_instances, 0);

	{
		test_ptr p1;
		p1 = get_test_rc();
		test_ptr p2 = p1;
		CPPA_CHECK_EQUAL(rc_instances, 1);
		CPPA_CHECK_EQUAL(p1->rc(), 2);
	}
	CPPA_CHECK_EQUAL(rc_instances, 0);

	{
		std::list<test_ptr> pl;
		pl.push_back(get_test_ptr());
		pl.push_back(get_test_rc());
		pl.push_back(pl.front()->create());
		CPPA_CHECK_EQUAL(pl.front()->rc(), 1);
		CPPA_CHECK_EQUAL(rc_instances, 3);
	}
	CPPA_CHECK_EQUAL(rc_instances, 0);

}
