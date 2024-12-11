// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/actor_cast.hpp"

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/test.hpp"

#include "caf/event_based_actor.hpp"
#include "caf/typed_event_based_actor.hpp"

using namespace caf;

namespace {

struct dummy_trait {
  using signatures = type_list<result<int>(int)>;
};

using dummy_actor = typed_actor<dummy_trait>;

behavior dummy_impl() {
  return {
    [](int x) { return x; },
  };
}

dummy_actor::behavior_type typed_dummy_impl() {
  return {
    [](int x) { return x; },
  };
}

struct access_tag {};

} // namespace

namespace caf {

// Abuse the fact that `actor_cast_access` has friend access to the handles.
template <>
class actor_cast_access<access_tag, access_tag, -1> {
public:
  static auto& unbox(actor& x) {
    return x.ptr_;
  }

  template <class... Ts>
  static auto& unbox(typed_actor<Ts...>& x) {
    return x.ptr_;
  }

  static auto& unbox(actor_addr& x) {
    return x.ptr_;
  }
};

using access_t = actor_cast_access<access_tag, access_tag, -1>;

} // namespace caf

WITH_FIXTURE(test::fixture::deterministic) {

TEST("actor_cast converts a strong pointer to a weak  pointer") {
  SECTION("valid handle") {
    SECTION("actor") {
      auto hdl = sys.spawn(dummy_impl);
      auto ptr = actor_cast<weak_actor_ptr>(hdl);
      check_eq(access_t::unbox(hdl).get(), ptr.get());
    }
    SECTION("typed_actor") {
      auto hdl = sys.spawn(typed_dummy_impl);
      auto ptr = actor_cast<weak_actor_ptr>(hdl);
      check_eq(access_t::unbox(hdl).get(), ptr.get());
    }
  }
  SECTION("invalid handle") {
    SECTION("actor") {
      auto hdl = actor{};
      auto ptr = actor_cast<weak_actor_ptr>(hdl);
      check_eq(ptr, nullptr);
    }
    SECTION("typed_actor") {
      auto hdl = dummy_actor{};
      auto ptr = actor_cast<weak_actor_ptr>(hdl);
      check_eq(ptr, nullptr);
    }
  }
}

TEST("actor_cast converts a weak pointer to a strong pointer") {
  SECTION("valid handle") {
    SECTION("actor") {
      auto hdl = sys.spawn(dummy_impl);
      auto addr = hdl.address();
      auto hdl2 = actor_cast<actor>(addr);
      check_eq(access_t::unbox(addr).get(), access_t::unbox(hdl2));
    }
    SECTION("typed_actor") {
      auto hdl = sys.spawn(typed_dummy_impl);
      auto addr = hdl.address();
      auto hdl2 = actor_cast<dummy_actor>(addr);
      check_eq(access_t::unbox(addr).get(), access_t::unbox(hdl2));
    }
  }
  SECTION("expired handle") {
    SECTION("actor") {
      auto addr = sys.spawn(dummy_impl).address();
      auto hdl2 = actor_cast<actor>(addr);
      check_eq(access_t::unbox(hdl2).get(), nullptr);
    }
    SECTION("typed_actor") {
      auto addr = sys.spawn(typed_dummy_impl).address();
      auto hdl2 = actor_cast<dummy_actor>(addr);
      check_eq(access_t::unbox(hdl2).get(), nullptr);
    }
  }
  SECTION("invalid handle") {
    SECTION("actor") {
      auto addr = actor_addr{};
      auto hdl2 = actor_cast<actor>(addr);
      check_eq(access_t::unbox(hdl2).get(), nullptr);
    }
    SECTION("typed_actor") {
      auto addr = actor_addr{};
      auto hdl2 = actor_cast<dummy_actor>(addr);
      check_eq(access_t::unbox(hdl2).get(), nullptr);
    }
  }
}

TEST("actor_cast converts a strong pointer to an actor_control_block*") {
  SECTION("valid handle") {
    SECTION("actor") {
      auto hdl = sys.spawn(dummy_impl);
      auto addr = hdl.address();
      auto ptr = actor_cast<actor_control_block*>(addr);
      check_eq(access_t::unbox(addr).get(), ptr);
    }
    SECTION("typed_actor") {
      auto hdl = sys.spawn(typed_dummy_impl);
      auto addr = hdl.address();
      auto ptr = actor_cast<actor_control_block*>(addr);
      check_eq(access_t::unbox(addr).get(), ptr);
    }
  }
  SECTION("invalid handle") {
    SECTION("actor") {
      auto hdl = actor{};
      auto addr = hdl.address();
      auto ptr = actor_cast<actor_control_block*>(addr);
      check_eq(ptr, nullptr);
    }
    SECTION("typed_actor") {
      auto hdl = dummy_actor{};
      auto addr = hdl.address();
      auto ptr = actor_cast<actor_control_block*>(addr);
      check_eq(ptr, nullptr);
    }
  }
}

TEST("actor_cast converts a strong pointer to an abstract_actor*") {
  SECTION("valid handle") {
    SECTION("actor") {
      auto hdl = sys.spawn(dummy_impl);
      auto addr = hdl.address();
      auto ptr = actor_cast<abstract_actor*>(addr);
      if (check_ne(ptr, nullptr)) {
        check_eq(access_t::unbox(addr).get(), ptr->ctrl());
      }
    }
    SECTION("typed_actor") {
      auto hdl = sys.spawn(typed_dummy_impl);
      auto addr = hdl.address();
      auto ptr = actor_cast<abstract_actor*>(addr);
      if (check_ne(ptr, nullptr)) {
        check_eq(access_t::unbox(addr).get(), ptr->ctrl());
      }
    }
  }
  SECTION("invalid handle") {
    SECTION("actor") {
      auto hdl = actor{};
      auto addr = hdl.address();
      auto ptr = actor_cast<abstract_actor*>(addr);
      check_eq(ptr, nullptr);
    }
    SECTION("typed_actor") {
      auto hdl = dummy_actor{};
      auto addr = hdl.address();
      auto ptr = actor_cast<abstract_actor*>(addr);
      check_eq(ptr, nullptr);
    }
  }
}

TEST("actor_cast converts a strong pointer to a local_actor*") {
  SECTION("valid handle") {
    SECTION("actor") {
      auto hdl = sys.spawn(dummy_impl);
      auto addr = hdl.address();
      auto ptr = actor_cast<local_actor*>(addr);
      if (check_ne(ptr, nullptr)) {
        check_eq(access_t::unbox(addr).get(), ptr->ctrl());
      }
    }
    SECTION("typed_actor") {
      auto hdl = sys.spawn(typed_dummy_impl);
      auto addr = hdl.address();
      auto ptr = actor_cast<local_actor*>(addr);
      if (check_ne(ptr, nullptr)) {
        check_eq(access_t::unbox(addr).get(), ptr->ctrl());
      }
    }
  }
  SECTION("invalid handle") {
    SECTION("actor") {
      auto hdl = actor{};
      auto addr = hdl.address();
      auto ptr = actor_cast<local_actor*>(addr);
      check_eq(ptr, nullptr);
    }
    SECTION("typed_actor") {
      auto hdl = dummy_actor{};
      auto addr = hdl.address();
      auto ptr = actor_cast<local_actor*>(addr);
      check_eq(ptr, nullptr);
    }
  }
}

TEST("actor_cast converts a self pointer to a handle type") {
  SECTION("actor") {
    sys.spawn([this](event_based_actor* self) {
      auto hdl = actor_cast<actor>(self);
      check_eq(self->ctrl(), access_t::unbox(hdl));
    });
    auto* ptr = static_cast<event_based_actor*>(nullptr);
    auto hdl = actor_cast<actor>(ptr);
    check_eq(access_t::unbox(hdl), nullptr);
  }
  SECTION("typed_actor") {
    sys.spawn([this](dummy_actor::pointer self) {
      auto hdl = actor_cast<dummy_actor>(self);
      check_eq(self->ctrl(), access_t::unbox(hdl));
      return dummy_actor::behavior_type{
        [](int x) { return x; },
      };
    });
    auto* ptr = static_cast<dummy_actor::pointer>(nullptr);
    auto hdl = actor_cast<actor>(ptr);
    check_eq(access_t::unbox(hdl), nullptr);
  }
}

} // WITH_FIXTURE(test::fixture::deterministic)
