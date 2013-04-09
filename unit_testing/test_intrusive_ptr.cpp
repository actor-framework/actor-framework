#include <list>
#include <cstddef>

#include "test.hpp"
#include "cppa/ref_counted.hpp"
#include "cppa/intrusive_ptr.hpp"

using namespace cppa;

namespace {

int class0_instances = 0;
int class1_instances = 0;

}

struct class0 : ref_counted {
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

int main() {
    // this test dosn't test thread-safety of intrusive_ptr
    // however, it is thread safe since it uses atomic operations only

    CPPA_TEST(test_intrusive_ptr);
    {
        auto p = make_counted<class0>();
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
        auto p1 = make_counted<class0>();
        p1 = new class1;
        CPPA_CHECK_EQUAL(class0_instances, 1);
        CPPA_CHECK_EQUAL(class1_instances, 1);
        auto p2 = make_counted<class1>();
        p1 = p2;
        CPPA_CHECK_EQUAL(class0_instances, 1);
        CPPA_CHECK_EQUAL(class1_instances, 1);
        CPPA_CHECK(p1 == p2);
    }
    CPPA_CHECK_EQUAL(class0_instances, 0);
    CPPA_CHECK_EQUAL(class1_instances, 0);

    return CPPA_TEST_RESULT();

}
