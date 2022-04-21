#include "caf/all.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>

enum class fixed_stack_errc : uint8_t {
  push_to_full = 1,
  pop_from_empty,
};

CAF_BEGIN_TYPE_ID_BLOCK(fixed_stack, first_custom_type_id)

  CAF_ADD_TYPE_ID(fixed_stack, (fixed_stack_errc))

  CAF_ADD_ATOM(fixed_stack, pop_atom)
  CAF_ADD_ATOM(fixed_stack, push_atom)

CAF_END_TYPE_ID_BLOCK(fixed_stack)

CAF_ERROR_CODE_ENUM(fixed_stack_errc)

std::string to_string(fixed_stack_errc x) {
  switch (x) {
    case fixed_stack_errc::push_to_full:
      return "push_to_full";
    case fixed_stack_errc::pop_from_empty:
      return "pop_from_empty";
    default:
      return "-unknown-error-";
  }
}

bool from_string(std::string_view in, fixed_stack_errc& out) {
  if (in == "push_to_full") {
    out = fixed_stack_errc::push_to_full;
    return true;
  } else if (in == "pop_from_empty") {
    out = fixed_stack_errc::pop_from_empty;
    return true;
  } else {
    return false;
  }
}

bool from_integer(uint8_t in, fixed_stack_errc& out) {
  if (in > 0 && in < 3) {
    out = static_cast<fixed_stack_errc>(in);
    return true;
  } else {
    return false;
  }
}

template <class Inspector>
bool inspect(Inspector& f, fixed_stack_errc& x) {
  return caf::default_enum_inspect(f, x);
}

using std::endl;
using namespace caf;

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
    assert(size_ >= 2);
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

CAF_MAIN(id_block::fixed_stack)
