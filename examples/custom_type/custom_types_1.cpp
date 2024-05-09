// Showcases how to add custom POD message types.

#include "caf/actor_from_state.hpp"
#include "caf/actor_ostream.hpp"
#include "caf/actor_system.hpp"
#include "caf/caf_main.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/type_id.hpp"

#include <cassert>
#include <string>
#include <utility>
#include <vector>

// --(rst-type-id-block-begin)--
struct foo;
struct foo2;

CAF_BEGIN_TYPE_ID_BLOCK(custom_types_1, first_custom_type_id)

  CAF_ADD_TYPE_ID(custom_types_1, (foo))
  CAF_ADD_TYPE_ID(custom_types_1, (foo2))
  CAF_ADD_TYPE_ID(custom_types_1, (std::pair<int32_t, int32_t>) )

CAF_END_TYPE_ID_BLOCK(custom_types_1)
// --(rst-type-id-block-end)--

using std::endl;

using namespace caf;

// --(rst-foo-begin)--
struct foo {
  std::vector<int> a;
  int b;
};

template <class Inspector>
bool inspect(Inspector& f, foo& x) {
  return f.object(x).fields(f.field("a", x.a), f.field("b", x.b));
}
// --(rst-foo-end)--

// a pair of two ints
using foo_pair = std::pair<int, int>;

// another alias for pairs of two ints
using foo_pair2 = std::pair<int, int>;

// a struct with a nested container
struct foo2 {
  int a;
  std::vector<std::vector<double>> b;
};

// foo2 also needs to be serializable
template <class Inspector>
bool inspect(Inspector& f, foo2& x) {
  return f.object(x).fields(f.field("a", x.a), f.field("b", x.b));
}

struct testee_state {
  testee_state(event_based_actor* selfptr, size_t remaining_messages)
    : self(selfptr), remaining(remaining_messages) {
    // nop
  }

  behavior make_behavior() {
    if (remaining == 0)
      return {};
    return {
      // note: we sent a foo_pair2, but match on foo_pair
      // that works because both are aliases for std::pair<int, int>
      [this](const foo_pair& val) {
        self->println("foo_pair{}", val);
        if (--remaining == 0)
          self->quit();
      },
      [this](const foo& val) {
        self->println("{}", val);
        if (--remaining == 0)
          self->quit();
      },
    };
  }

  event_based_actor* self;
  size_t remaining;
};

void caf_main(actor_system& sys) {
  // two variables for testing serialization
  foo2 f1;
  foo2 f2;
  // init some test data
  f1.a = 5;
  f1.b.resize(1);
  f1.b.back().push_back(42);
  // byte buffer
  binary_serializer::container_type buf;
  // write f1 to buffer
  binary_serializer sink{sys, buf};
  if (!sink.apply(f1)) {
    sys.println("*** failed to serialize foo2: {}", sink.get_error());
    return;
  }
  // read f2 back from buffer
  binary_deserializer source{sys, buf};
  if (!source.apply(f2)) {
    sys.println("*** failed to deserialize foo2: {}", source.get_error());
    return;
  }
  // must be equal
  assert(deep_to_string(f1) == deep_to_string(f2));
  // spawn a testee that receives two messages of user-defined types
  auto t = sys.spawn(actor_from_state<testee_state>, 2u);
  scoped_actor self{sys};
  // send t a foo
  self->mail(foo{std::vector<int>{1, 2, 3, 4}, 5}).send(t);
  // send t a foo_pair2
  self->mail(foo_pair2{3, 4}).send(t);
}

CAF_MAIN(id_block::custom_types_1)
