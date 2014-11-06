#include <vector>

#include "test.hpp"

#include "caf/all.hpp"

using namespace caf;

class fixed_stack : public sb_actor<fixed_stack> {
 public:
  friend class sb_actor<fixed_stack>;
  fixed_stack(size_t max) : max_size(max)  {
    full = (
      on(atom("push"), arg_match) >> [=](int) { /* discard */ },
      on(atom("pop")) >> [=]() -> cow_tuple<atom_value, int> {
        auto result = data.back();
        data.pop_back();
        become(filled);
        return {atom("ok"), result};
      }
    );
    filled = (
      on(atom("push"), arg_match) >> [=](int what) {
        data.push_back(what);
        if (data.size() == max_size) become(full);
      },
      on(atom("pop")) >> [=]() -> cow_tuple<atom_value, int> {
        auto result = data.back();
        data.pop_back();
        if (data.empty()) become(empty);
        return {atom("ok"), result};
      }
    );
    empty = (
      on(atom("push"), arg_match) >> [=](int what) {
        data.push_back(what);
        become(filled);
      },
      on(atom("pop")) >> [=] {
        return atom("failure");
      }
    );
  }
 private:
  size_t max_size = 10;
  std::vector<int> data;
  behavior full;
  behavior filled;
  behavior empty;
  behavior& init_state = empty;
};

void test_fixed_stack_actor() {
  scoped_actor self;
  auto st = spawn<fixed_stack>(size_t{10});
  // push 20 values
  for (int i = 0; i < 20; ++i) self->send(st, atom("push"), i);
  // pop 20 times
  for (int i = 0; i < 20; ++i) self->send(st, atom("pop"));
  // expect 10 failure messages
  {
    int i = 0;
    self->receive_for(i, 10) (
      on(atom("failure")) >> CAF_CHECKPOINT_CB()
    );
    CAF_CHECKPOINT();
  }
  // expect 10 {'ok', value} messages
  {
    std::vector<int> values;
    int i = 0;
    self->receive_for(i, 10) (
      on(atom("ok"), arg_match) >> [&](int value) {
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
  return CAF_TEST_RESULT();
}
