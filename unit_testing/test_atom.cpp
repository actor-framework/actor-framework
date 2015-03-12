#include <string>
#include <typeinfo>
#include <iostream>

#include "test.hpp"

#include "caf/all.hpp"
#include "caf/scoped_actor.hpp"

namespace caf {
inline std::ostream& operator<<(std::ostream& out, const atom_value& a) {
  return (out << to_string(a));
}
} // namespace caf

using std::cout;
using std::endl;
using std::string;

using namespace caf;

namespace {
constexpr auto s_foo = atom("FooBar");
using abc_atom = atom_constant<atom("abc")>;
}

using testee = typed_actor<replies_to<abc_atom>::with<int>>;

testee::behavior_type testee_impl(testee::pointer self) {
  return {
    [=](abc_atom) {
      CAF_CHECKPOINT();
      self->quit();
      return 42;
    }
  };
}

void test_typed_atom_interface() {
  CAF_CHECKPOINT();
  scoped_actor self;
  auto tst = spawn_typed(testee_impl);
  self->sync_send(tst, abc_atom::value).await(
    [](int i) {
      CAF_CHECK_EQUAL(i, 42);
    },
    CAF_UNEXPECTED_MSG_CB_REF(self)
  );
}

template <atom_value AtomValue, class... Types>
void foo() {
  CAF_PRINT("foo(" << static_cast<uint64_t>(AtomValue) << " = "
            << to_string(AtomValue) << ")");
}

struct send_to_self {
  send_to_self(blocking_actor* self) : m_self(self) {
    // nop
  }
  template <class... Ts>
  void operator()(Ts&&... xs) {
    m_self->send(m_self, std::forward<Ts>(xs)...);
  }
  blocking_actor* m_self;
};

int main() {
  CAF_TEST(test_atom);
  // check if there are leading bits that distinguish "zzz" and "000 "
  CAF_CHECK_NOT_EQUAL(atom("zzz"), atom("000 "));
  // check if there are leading bits that distinguish "abc" and " abc"
  CAF_CHECK_NOT_EQUAL(atom("abc"), atom(" abc"));
  // 'illegal' characters are mapped to whitespaces
  CAF_CHECK_EQUAL(atom("   "), atom("@!?"));
  // check to_string impl.
  CAF_CHECK_EQUAL(to_string(s_foo), "FooBar");
  scoped_actor self;
  send_to_self f{self.get()};
  f(atom("foo"), static_cast<uint32_t>(42));
  f(atom(":Attach"), atom(":Baz"), "cstring");
  f(1.f);
  f(atom("a"), atom("b"), atom("c"), 23.f);
  bool matched_pattern[3] = {false, false, false};
  int i = 0;
  CAF_CHECKPOINT();
  for (i = 0; i < 3; ++i) {
    self->receive(
      on(atom("foo"), arg_match) >> [&](uint32_t value) {
        CAF_CHECKPOINT();
        matched_pattern[0] = true;
        CAF_CHECK_EQUAL(value, 42);
      },
      on(atom(":Attach"), atom(":Baz"), arg_match) >> [&](const string& str) {
        CAF_CHECKPOINT();
        matched_pattern[1] = true;
        CAF_CHECK_EQUAL(str, "cstring");
      },
      on(atom("a"), atom("b"), atom("c"), arg_match) >> [&](float value) {
        CAF_CHECKPOINT();
        matched_pattern[2] = true;
        CAF_CHECK_EQUAL(value, 23.f);
      });
  }
  CAF_CHECK(matched_pattern[0] && matched_pattern[1] && matched_pattern[2]);
  self->receive(
    // "erase" message { atom("b"), atom("a"), atom("c"), 23.f }
    others >> CAF_CHECKPOINT_CB(),
    after(std::chrono::seconds(0)) >> CAF_UNEXPECTED_TOUT_CB()
  );
  atom_value x = atom("abc");
  atom_value y = abc_atom::value;
  CAF_CHECK_EQUAL(x, y);
  auto msg = make_message(abc_atom());
  self->send(self, msg);
  self->receive(
    on(atom("abc")) >> CAF_CHECKPOINT_CB(),
    others >> CAF_UNEXPECTED_MSG_CB_REF(self)
  );
  test_typed_atom_interface();
  shutdown();
  return CAF_TEST_RESULT();
}
