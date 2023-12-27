// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/actor_from_state.hpp"

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/test.hpp"

#include "caf/typed_actor.hpp"

using namespace caf;

namespace {

behavior dummy_impl() {
  return {
    [](int) { return; },
    [](uint64_t) { return; },
  };
}

WITH_FIXTURE(test::fixture::deterministic) {

struct cell_state {
  cell_state() = default;

  explicit cell_state(int init) : value(init) {
    // nop
  }

  behavior make_behavior() {
    return {
      [this](get_atom) { return value; },
      [this](put_atom, int new_value) { value = new_value; },
    };
  }

  int value = 0;
};

using typed_cell_actor
  = typed_actor<result<int>(get_atom), result<void>(put_atom, int)>;

struct typed_cell_state {
  typed_cell_state() = default;

  explicit typed_cell_state(int init) : value(init) {
    // nop
  }

  typed_cell_actor::behavior_type make_behavior() {
    return {
      [this](get_atom) { return value; },
      [this](put_atom, int new_value) { value = new_value; },
    };
  }

  int value = 0;
};

TEST("a default-constructed cell has value 0") {
  auto dummy = sys.spawn(dummy_impl);
  SECTION("dynamically typed") {
    auto uut = sys.spawn(actor_from_state<cell_state>);
    static_assert(std::is_same_v<decltype(uut), actor>);
    inject().with(get_atom_v).from(dummy).to(uut);
    expect<int>().with(0).from(uut).to(dummy);
    inject().with(put_atom_v, 23).from(dummy).to(uut);
    inject().with(get_atom_v).from(dummy).to(uut);
    expect<int>().with(23).from(uut).to(dummy);
  }
  SECTION("statically typed") {
    auto uut = sys.spawn(actor_from_state<typed_cell_state>);
    static_assert(std::is_same_v<decltype(uut), typed_cell_actor>);
    inject().with(get_atom_v).from(dummy).to(uut);
    expect<int>().with(0).from(uut).to(dummy);
    inject().with(put_atom_v, 23).from(dummy).to(uut);
    inject().with(get_atom_v).from(dummy).to(uut);
    expect<int>().with(23).from(uut).to(dummy);
  }
}

TEST("passing a value to the cell constructor overrides the default value") {
  auto dummy = sys.spawn(dummy_impl);
  SECTION("dynamically typed") {
    auto uut = sys.spawn(actor_from_state<cell_state>, 42);
    static_assert(std::is_same_v<decltype(uut), actor>);
    inject().with(get_atom_v).from(dummy).to(uut);
    expect<int>().with(42).from(uut).to(dummy);
    inject().with(put_atom_v, 23).from(dummy).to(uut);
    inject().with(get_atom_v).from(dummy).to(uut);
    expect<int>().with(23).from(uut).to(dummy);
  }
  SECTION("statically typed") {
    auto uut = sys.spawn(actor_from_state<typed_cell_state>, 42);
    static_assert(std::is_same_v<decltype(uut), typed_cell_actor>);
    inject().with(get_atom_v).from(dummy).to(uut);
    expect<int>().with(42).from(uut).to(dummy);
    inject().with(put_atom_v, 23).from(dummy).to(uut);
    inject().with(get_atom_v).from(dummy).to(uut);
    expect<int>().with(23).from(uut).to(dummy);
  }
}

struct id_cell_state {
  explicit id_cell_state(event_based_actor* selfptr, uint64_t offset_init = 0)
    : self(selfptr), offset(offset_init) {
    // nop
  }

  behavior make_behavior() {
    return {
      [this](get_atom) { return self->id() + offset; },
    };
  }

  event_based_actor* self;
  uint64_t offset;
};

using typed_id_cell_actor = typed_actor<result<uint64_t>(get_atom)>;

struct typed_id_cell_state {
  explicit typed_id_cell_state(typed_id_cell_actor::pointer selfptr,
                               uint64_t offset_init = 0)
    : self(selfptr), offset(offset_init) {
    // nop
  }

  typed_id_cell_actor::behavior_type make_behavior() {
    return {
      [this](get_atom) { return self->id() + offset; },
    };
  }

  typed_id_cell_actor::pointer self;
  uint64_t offset;
};

TEST("the state may take the self pointer as constructor argument") {
  auto dummy = sys.spawn(dummy_impl);
  SECTION("no additional constructor argument") {
    SECTION("dynamically typed") {
      auto uut = sys.spawn(actor_from_state<id_cell_state>);
      static_assert(std::is_same_v<decltype(uut), actor>);
      inject().with(get_atom_v).from(dummy).to(uut);
      expect<uint64_t>().with(uut->id()).from(uut).to(dummy);
    }
    SECTION("statically typed") {
      auto uut = sys.spawn(actor_from_state<typed_id_cell_state>);
      static_assert(std::is_same_v<decltype(uut), typed_id_cell_actor>);
      inject().with(get_atom_v).from(dummy).to(uut);
      expect<uint64_t>().with(uut->id()).from(uut).to(dummy);
    }
  }
  SECTION("with offset constructor argument") {
    SECTION("dynamically typed") {
      auto uut = sys.spawn(actor_from_state<id_cell_state>, 2u);
      static_assert(std::is_same_v<decltype(uut), actor>);
      inject().with(get_atom_v).from(dummy).to(uut);
      expect<uint64_t>().with(uut->id() + 2).from(uut).to(dummy);
    }
    SECTION("statically typed") {
      auto uut = sys.spawn(actor_from_state<typed_id_cell_state>, 2u);
      static_assert(std::is_same_v<decltype(uut), typed_id_cell_actor>);
      inject().with(get_atom_v).from(dummy).to(uut);
      expect<uint64_t>().with(uut->id() + 2).from(uut).to(dummy);
    }
  }
}

} // WITH_FIXTURE(test::fixture::deterministic)

} // namespace
