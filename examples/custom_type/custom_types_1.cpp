// Showcases how to add custom POD message types.

// Manual refs: 24-27, 30-34, 75-78, 81-84 (ConfiguringActorApplications)
//              23-33 (TypeInspection)

#include <string>
#include <vector>
#include <cassert>
#include <utility>
#include <iostream>

#include "caf/all.hpp"

// --(rst-type-id-block-begin)--
struct foo;
struct foo2;

CAF_BEGIN_TYPE_ID_BLOCK(custom_types_1, first_custom_type_id)

  CAF_ADD_TYPE_ID(custom_types_1, (foo))
  CAF_ADD_TYPE_ID(custom_types_1, (foo2))
  CAF_ADD_TYPE_ID(custom_types_1, (std::pair<int32_t, int32_t>) )

CAF_END_TYPE_ID_BLOCK(custom_types_1)
// --(rst-type-id-block-end)--

using std::cout;
using std::cerr;
using std::endl;
using std::vector;

using namespace caf;

// --(rst-foo-begin)--
struct foo {
  std::vector<int> a;
  int b;
};

template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, foo& x) {
  return f(meta::type_name("foo"), x.a, x.b);
}
// --(rst-foo-end)--

// a pair of two ints
using foo_pair = std::pair<int, int>;

// another alias for pairs of two ints
using foo_pair2 = std::pair<int, int>;

// a struct with a nested container
struct foo2 {
  int a;
  vector<vector<double>> b;
};

// foo2 also needs to be serializable
template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, foo2& x) {
  return f(meta::type_name("foo2"), x.a, x.b);
}

// receives our custom message types
void testee(event_based_actor* self, size_t remaining) {
  auto set_next_behavior = [=] {
    if (remaining > 1)
      testee(self, remaining - 1);
    else
      self->quit();
  };
  self->become (
    // note: we sent a foo_pair2, but match on foo_pair
    // that works because both are aliases for std::pair<int, int>
    [=](const foo_pair& val) {
      aout(self) << "foo_pair" << deep_to_string(val) << endl;
      set_next_behavior();
    },
    [=](const foo& val) {
      aout(self) << to_string(val) << endl;
      set_next_behavior();
    }
  );
}

// --(rst-config-begin)--
class config : public actor_system_config {
public:
  config() {
    init_global_meta_objects<custom_types_1_type_ids>();
  }
};
// --(rst-config-end)--

void caf_main(actor_system& system, const config&) {
  // two variables for testing serialization
  foo2 f1;
  foo2 f2;
  // init some test data
  f1.a = 5;
  f1.b.resize(1);
  f1.b.back().push_back(42);
  // I/O buffer
  binary_serializer::container_type buf;
  // write f1 to buffer
  binary_serializer bs{system, buf};
  auto e = bs(f1);
  if (e) {
    std::cerr << "*** unable to serialize foo2: "
              << system.render(e) << std::endl;
    return;
  }
  // read f2 back from buffer
  binary_deserializer bd{system, buf};
  e = bd(f2);
  if (e) {
    std::cerr << "*** unable to serialize foo2: "
              << system.render(e) << std::endl;
    return;
  }
  // must be equal
  assert(to_string(f1) == to_string(f2));
  // spawn a testee that receives two messages of user-defined type
  auto t = system.spawn(testee, 2u);
  scoped_actor self{system};
  // send t a foo
  self->send(t, foo{std::vector<int>{1, 2, 3, 4}, 5});
  // send t a foo_pair2
  self->send(t, foo_pair2{3, 4});
}

CAF_MAIN()
