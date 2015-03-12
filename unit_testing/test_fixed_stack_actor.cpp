#include <tuple>
#include <vector>

#include "test.hpp"

#include "caf/all.hpp"

using namespace caf;

using pop_atom = atom_constant<atom("pop")>;
using push_atom = atom_constant<atom("push")>;

class fixed_stack : public sb_actor<fixed_stack> {
 public:
  friend class sb_actor<fixed_stack>;
  fixed_stack(size_t max) : max_size(max)  {
    full.assign(
      [=](push_atom, int) {
        /* discard */
      },
      [=](pop_atom) -> message {
        auto result = data.back();
        data.pop_back();
        become(filled);
        return make_message(ok_atom::value, result);
      }
    );
    filled.assign(
      [=](push_atom, int what) {
        data.push_back(what);
        if (data.size() == max_size) {
          become(full);
        }
      },
      [=](pop_atom) -> message {
        auto result = data.back();
        data.pop_back();
        if (data.empty()) {
          become(empty);
        }
        return make_message(ok_atom::value, result);
      }
    );
    empty.assign(
      [=](push_atom, int what) {
        data.push_back(what);
        become(filled);
      },
      [=](pop_atom) {
        return error_atom::value;
      }
    );
  }

  ~fixed_stack();

 private:
  size_t max_size = 10;
  std::vector<int> data;
  behavior full;
  behavior filled;
  behavior empty;
  behavior& init_state = empty;
};

fixed_stack::~fixed_stack() {
  // avoid weak-vtables warning
}

void test_fixed_stack_actor() {
  scoped_actor self;
  auto st = spawn<fixed_stack>(size_t{10});
  // push 20 values
  for (int i = 0; i < 20; ++i) self->send(st, push_atom::value, i);
  // pop 20 times
  for (int i = 0; i < 20; ++i) self->send(st, pop_atom::value);
  // expect 10 failure messages
  {
    int i = 0;
    self->receive_for(i, 10) (
      [](error_atom) {
        CAF_CHECKPOINT();
      }
    );
    CAF_CHECKPOINT();
  }
  // expect 10 {'ok', value} messages
  {
    std::vector<int> values;
    int i = 0;
    self->receive_for(i, 10) (
      [&](ok_atom, int value) {
        values.push_back(value);
      }
    );
    std::vector<int> expected{9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
    CAF_CHECK_EQUAL(join(values, ","), join(expected, ","));
  }
  // terminate st
  self->send_exit(st, exit_reason::user_shutdown);
  self->await_all_other_actors_done();
}

int main() {
  CAF_TEST(test_fixed_stack_actor);
  test_fixed_stack_actor();
  shutdown();
  return CAF_TEST_RESULT();
}
