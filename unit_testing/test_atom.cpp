#include <string>
#include <typeinfo>
#include <iostream>

#include "test.hpp"

#include "cppa/all.hpp"
#include "cppa/scoped_actor.hpp"

namespace cppa {
inline std::ostream& operator<<(std::ostream& out, const atom_value& a) {
    return (out << to_string(a));
}
} // namespace cppa

using std::cout;
using std::endl;
using std::string;

using namespace cppa;

namespace {
constexpr auto s_foo = atom("FooBar");
}

template<atom_value AtomValue, typename... Types>
void foo() {
    CPPA_PRINT("foo(" << static_cast<uint64_t>(AtomValue) << " = "
                      << to_string(AtomValue) << ")");
}

struct send_to_self {
    send_to_self(blocking_actor* self) : m_self(self) {}
    template<typename... Ts>
    void operator()(Ts&&... args) {
        m_self->send(m_self, std::forward<Ts>(args)...);
    }
    blocking_actor* m_self;

};

int main() {
    CPPA_TEST(test_atom);
    // check if there are leading bits that distinguish "zzz" and "000 "
    CPPA_CHECK_NOT_EQUAL(atom("zzz"), atom("000 "));
    // check if there are leading bits that distinguish "abc" and " abc"
    CPPA_CHECK_NOT_EQUAL(atom("abc"), atom(" abc"));
    // 'illegal' characters are mapped to whitespaces
    CPPA_CHECK_EQUAL(atom("   "), atom("@!?"));
    // check to_string impl.
    CPPA_CHECK_EQUAL(to_string(s_foo), "FooBar");
    scoped_actor self;
    send_to_self f{self.get()};
    f(atom("foo"), static_cast<uint32_t>(42));
    f(atom(":Attach"), atom(":Baz"), "cstring");
    // m(atom("b"), atom("a"), atom("c"), 23.f, 1.f, 1.f);
    f(1.f);
    f(atom("a"), atom("b"), atom("c"), 23.f);
    bool matched_pattern[3] = {false, false, false};
    int i = 0;
    for (i = 0; i < 3; ++i) {
        self->receive(
            //}
            // self->receive_for(i, 3) (
            on(atom("foo"), arg_match) >> [&](uint32_t value) {
                CPPA_CHECKPOINT();
                matched_pattern[0] = true;
                CPPA_CHECK_EQUAL(value, 42);
            },
            on(atom(":Attach"), atom(":Baz"), arg_match) >>
                [&](const string& str) {
                CPPA_CHECKPOINT();
                matched_pattern[1] = true;
                CPPA_CHECK_EQUAL(str, "cstring");
            },
            on(atom("a"), atom("b"), atom("c"), arg_match) >> [&](float value) {
                CPPA_CHECKPOINT();
                matched_pattern[2] = true;
                CPPA_CHECK_EQUAL(value, 23.f);
            });
    }
    CPPA_CHECK(matched_pattern[0] && matched_pattern[1] && matched_pattern[2]);
    self->receive(
        // "erase" message { atom("b"), atom("a"), atom("c"), 23.f }
        others() >> CPPA_CHECKPOINT_CB(),
        after(std::chrono::seconds(0)) >> CPPA_UNEXPECTED_TOUT_CB());
    return CPPA_TEST_RESULT();
}
