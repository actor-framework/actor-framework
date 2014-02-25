#include <iostream>
#include <functional>

#include "test.hpp"
#include "cppa/cppa.hpp"
#include "cppa/optional_variant.hpp"

using namespace std;
using namespace cppa;

namespace { std::atomic<size_t> some_struct_instances; }

struct some_struct {

    some_struct() { ++some_struct_instances; }

    some_struct(const some_struct&) { ++some_struct_instances; }

    ~some_struct() { --some_struct_instances; }

};

struct int_visitor {
    int operator()(none_t) const { return -1; }
    int operator()(int i) const { return i; }
    int operator()(const some_struct&) const { return 0; }
};

using dlimits = std::numeric_limits<double>;

struct double_visitor {
    double operator()(none_t) const { return dlimits::signaling_NaN(); }
    double operator()() const { return dlimits::quiet_NaN(); }
    template<typename T>
    double operator()(T value) const { return value; }
};

int main() {
    CPPA_TEST(test_optional_variant);

    /* run tets using primitive types */ {
        using tri_type = optional_variant<void, int, double, float>;

        tri_type t0;
        tri_type t1{unit};
        tri_type t2{0};
        tri_type t3{0.0};
        tri_type t4{0.0f};

        CPPA_CHECK(!t0);

        CPPA_CHECK(t1 && t1.is<void>());

        CPPA_CHECK(t2.is<int>());
        CPPA_CHECK(!t2.is<double>());
        CPPA_CHECK(!t2.is<float>());
        CPPA_CHECK_EQUAL(get<int>(t2), 0);
        get_ref<int>(t2) = 42;
        CPPA_CHECK_EQUAL(get<int>(t2), 42);

        CPPA_CHECK(!t3.is<int>());
        CPPA_CHECK(t3.is<double>());
        CPPA_CHECK(!t3.is<float>());
        CPPA_CHECK_EQUAL(get<double>(t3), 0.0);
        get_ref<double>(t3) = 4.2;
        CPPA_CHECK_EQUAL(get<double>(t3), 4.2);

        CPPA_CHECK(!t4.is<int>());
        CPPA_CHECK(!t4.is<double>());
        CPPA_CHECK(t4.is<float>());
        CPPA_CHECK_EQUAL(get<float>(t4), 0.0f);
        get_ref<float>(t4) = 2.3f;
        CPPA_CHECK_EQUAL(get<float>(t4), 2.3f);

        double_visitor dv;
        auto v = apply_visitor(dv, t0);
        CPPA_CHECK(v != v); // only true if v is NaN
        v = apply_visitor(dv, t1);
        CPPA_CHECK(v != v); // only true if v is NaN
        CPPA_CHECK_EQUAL(apply_visitor(dv, t2), 42.0);
        CPPA_CHECK_EQUAL(apply_visitor(dv, t3), 4.2);
        // converting 2.3f to double is not exactly 2.3
        CPPA_CHECK_EQUAL(apply_visitor(dv, t4), static_cast<double>(2.3f));

        t4 = 1;
        CPPA_CHECK(t4 && t4.is<int>() && get<int>(t4) == 1);

        t4 = none_t{};
        CPPA_CHECK(!t4);
    }

    /* run tests using user-defined types */ {
        using tri_type = optional_variant<int, some_struct>;
        tri_type t0;
        tri_type t1{42};
        CPPA_CHECK_EQUAL(some_struct_instances.load(), 0);
        tri_type t2{some_struct{}};
        CPPA_CHECK_EQUAL(some_struct_instances.load(), 1);
        CPPA_CHECK(!t0);
        CPPA_CHECK(t1 && t1.is<int>() && get<int>(t1) == 42);
        CPPA_CHECK(t2 && t2.is<some_struct>());
        int_visitor iVisit;
        CPPA_CHECK_EQUAL(apply_visitor(iVisit, t0), -1);
        CPPA_CHECK_EQUAL(apply_visitor(iVisit, t1), 42);
        CPPA_CHECK_EQUAL(apply_visitor(iVisit, t2), 0);
    }
    CPPA_CHECK_EQUAL(some_struct_instances.load(), 0);

    return CPPA_TEST_RESULT();
}
