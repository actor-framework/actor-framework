#include "test.hpp"

#include "cppa/primitive_variant.hpp"

namespace {

struct streamer {
    std::ostream& o;
    streamer(std::ostream& mo) : o(mo) { }
    template<typename T>
    void operator()(const T& value) {
        o << value;
    }
    void operator()(const std::u16string&) {
    }
    void operator()(const std::u32string&) {
    }
};

} // namespace <anonymous>


namespace cppa {
inline std::ostream& operator<<(std::ostream& o, const primitive_variant& pv) {
    streamer s{o};
    pv.apply(s);
    return o;
}
} // namespace cppa

using namespace cppa;

size_t test__primitive_variant() {
    CPPA_TEST(test__primitive_variant);

    std::uint32_t forty_two = 42;
    primitive_variant v1(forty_two);
    primitive_variant v2(pt_uint32);
    // type checking
    CPPA_CHECK_EQUAL(v1.ptype(), pt_uint32);
    CPPA_CHECK_EQUAL(v2.ptype(), pt_uint32);
    get_ref<std::uint32_t&>(v2) = forty_two;
    CPPA_CHECK(equal(v1, v2));
    CPPA_CHECK(equal(v1, forty_two));
    CPPA_CHECK(equal(forty_two, v2));
    // type mismatch => unequal
    CPPA_CHECK(!equal(v2, static_cast<std::int8_t>(forty_two)));
    v1 = "Hello world";
    CPPA_CHECK_EQUAL(v1.ptype(), pt_u8string);
    v2 = "Hello";
    CPPA_CHECK_EQUAL(v2.ptype(), pt_u8string);
    get_ref<std::string>(v2) += " world";
    CPPA_CHECK(equal(v1, v2));
    v2 = u"Hello World";
    CPPA_CHECK(!equal(v1, v2));

    return CPPA_TEST_RESULT;
}
