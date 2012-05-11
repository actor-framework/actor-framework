#include <list>
#include <cstddef>

#include "test.hpp"
#include "cppa/intrusive_ptr.hpp"

#include "cppa/detail/ref_counted_impl.hpp"

using namespace cppa;

namespace {

int class0_instances = 0;
int class1_instances = 0;

}

struct class0 : cppa::detail::ref_counted_impl<size_t> {
    class0() { ++class0_instances; }

    virtual ~class0() { --class0_instances; }

    virtual class0* create() const { return new class0; }
};

struct class1 : class0 {
    class1() { ++class1_instances; }

    virtual ~class1() { --class1_instances; }

    virtual class1* create() const { return new class1; }
};

typedef intrusive_ptr<class0> class0_ptr;
typedef intrusive_ptr<class1> class1_ptr;

class0* get_test_rc() {
    return new class0;
}

class0_ptr get_test_ptr() {
    return get_test_rc();
}

size_t test__intrusive_ptr() {
    // this test dosn't test thread-safety of intrusive_ptr
    // however, it is thread safe since it uses atomic operations only

    CPPA_TEST(test__intrusive_ptr);
 {
        class0_ptr p(new class0);
        CPPA_CHECK_EQUAL(class0_instances, 1);
        CPPA_CHECK(p->unique());
    }
    CPPA_CHECK_EQUAL(class0_instances, 0);
 {
        class0_ptr p;
        p = new class0;
        CPPA_CHECK_EQUAL(class0_instances, 1);
        CPPA_CHECK(p->unique());
    }
    CPPA_CHECK_EQUAL(class0_instances, 0);
 {
        class0_ptr p1;
        p1 = get_test_rc();
        class0_ptr p2 = p1;
        CPPA_CHECK_EQUAL(class0_instances, 1);
        CPPA_CHECK_EQUAL(p1->unique(), false);
    }
    CPPA_CHECK_EQUAL(class0_instances, 0);
 {
        std::list<class0_ptr> pl;
        pl.push_back(get_test_ptr());
        pl.push_back(get_test_rc());
        pl.push_back(pl.front()->create());
        CPPA_CHECK(pl.front()->unique());
        CPPA_CHECK_EQUAL(class0_instances, 3);
    }
    CPPA_CHECK_EQUAL(class0_instances, 0);
 {
        class0_ptr p1(new class0);
        p1 = new class1;
        CPPA_CHECK_EQUAL(class0_instances, 1);
        CPPA_CHECK_EQUAL(class1_instances, 1);
        class1_ptr p2(new class1);
        p1 = p2;
        CPPA_CHECK_EQUAL(class0_instances, 1);
        CPPA_CHECK_EQUAL(class1_instances, 1);
        CPPA_CHECK(p1 == p2);
    }
    CPPA_CHECK_EQUAL(class0_instances, 0);
    CPPA_CHECK_EQUAL(class1_instances, 0);

    return CPPA_TEST_RESULT;

}
