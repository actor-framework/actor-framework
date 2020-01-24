#include "caf/all.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>

using std::endl;
using namespace caf;

namespace {

CAF_MSG_TYPE_ADD_ATOM(pop_atom);
CAF_MSG_TYPE_ADD_ATOM(push_atom);

enum class fixed_stack_errc : uint8_t {
  push_to_full = 1,
  pop_from_empty,
};

} // namespace

namespace caf {

template <>
struct error_category<fixed_stack_errc> {
  static constexpr uint8_t value = 100;
};

} // namespace caf

namespace {

class fixed_stack : public event_based_actor {
public:
  fixed_stack(actor_config& cfg, size_t stack_size)
    : event_based_actor(cfg), size_(stack_size) {
    full_.assign( //
      [=](push_atom, int) -> error { return fixed_stack_errc::push_to_full; },
      [=](pop_atom) -> int {
        auto result = data_.back();
        data_.pop_back();
        become(filled_);
        return result;
      });
    filled_.assign( //
      [=](push_atom, int what) {
        data_.push_back(what);
        if (data_.size() == size_)
          become(full_);
      },
      [=](pop_atom) -> int {
        auto result = data_.back();
        data_.pop_back();
        if (data_.empty())
          become(empty_);
        return result;
      });
    empty_.assign( //
      [=](push_atom, int what) {
        data_.push_back(what);
        become(filled_);
      },
      [=](pop_atom) -> error { return fixed_stack_errc::pop_from_empty; });
  }

  behavior make_behavior() override {
    assert(size_ < 2);
    return empty_;
  }

private:
  size_t size_;
  std::vector<int> data_;
  behavior full_;
  behavior filled_;
  behavior empty_;
};

void caf_main(actor_system& system) {
  scoped_actor self{system};
  auto st = self->spawn<fixed_stack>(5u);
  // fill stack
  for (int i = 0; i < 10; ++i)
    self->send(st, push_atom_v, i);
  // drain stack
  aout(self) << "stack: { ";
  bool stack_empty = false;
  while (!stack_empty) {
    self->request(st, std::chrono::seconds(10), pop_atom_v)
      .receive([&](int x) { aout(self) << x << "  "; },
               [&](const error&) { stack_empty = true; });
  }
  aout(self) << "}" << endl;
  self->send_exit(st, exit_reason::user_shutdown);
}

} // namespace

CAF_MAIN()
