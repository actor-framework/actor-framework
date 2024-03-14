// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/blocking_mail.hpp"

#include "caf/test/test.hpp"

#include "caf/actor_system_config.hpp"
#include "caf/blocking_actor.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/typed_actor.hpp"
#include "caf/typed_event_based_actor.hpp"

using namespace caf;
using namespace std::literals;

namespace {

using dummy_actor = typed_actor<result<int>(int)>;

using dummy_behavior = dummy_actor::behavior_type;

struct config : actor_system_config {
  config() {
    set("caf.scheduler.max-threads", 2u);
  }
};

struct fixture {
  config cfg;
  actor_system sys{cfg};
};

WITH_FIXTURE(fixture) {

TEST("send request message") {
  scoped_actor self{sys};
  auto result = std::make_shared<int>(0);
  auto on_result = [result](int x) { *result = x; };
  auto err = std::make_shared<error>();
  auto on_error = [err](error x) { *err = x; };
  SECTION("valid response") {
    auto dummy = sys.spawn([]() -> dummy_behavior {
      return {
        [](int value) { return value * value; },
      };
    });
    SECTION("regular message") {
      self->mail(3).request(dummy, 1s).receive(on_result, on_error);
      check_eq(*result, 9);
    }
    SECTION("urgent message") {
      self->mail(3).urgent().request(dummy, 1s).receive(on_result, on_error);
      check_eq(*result, 9);
    }
  }
  SECTION("invalid response") {
    auto dummy = sys.spawn([]() -> behavior {
      return {
        [](int) { return "ok"s; },
      };
    });
    SECTION("regular message") {
      self->mail(3).request(dummy, 1s).receive(on_result, on_error);
      check_eq(*err, make_error(sec::unexpected_response));
    }
    SECTION("urgent message") {
      self->mail(3).urgent().request(dummy, 1s).receive(on_result, on_error);
      check_eq(*err, make_error(sec::unexpected_response));
    }
  }
  SECTION("no response") {
    auto dummy = sys.spawn([](event_based_actor* self) -> behavior {
      auto res = std::make_shared<response_promise>();
      return {
        [self, res](int) { *res = self->make_response_promise(); },
      };
    });
    self->mail(3).request(dummy, 10ms).receive(on_result, on_error);
    check_eq(*err, make_error(sec::request_timeout));
    self->mail(exit_msg{self->address(), exit_reason::user_shutdown})
      .send(dummy);
  }
}

TEST("send delayed request message") {
  scoped_actor self{sys};
  auto result = std::make_shared<int>(0);
  auto on_result = [result](int x) { *result = x; };
  auto err = std::make_shared<error>();
  auto on_error = [err](error x) { *err = x; };
  SECTION("valid response") {
    auto dummy = sys.spawn([]() -> dummy_behavior {
      return {
        [](int value) { return value * value; },
      };
    });
    SECTION("regular message") {
      self->mail(3).delay(5ms).request(dummy, 1s).receive(on_result, on_error);
      check_eq(*result, 9);
    }
    SECTION("urgent message") {
      self->mail(3)
        .urgent()
        .schedule(self->clock().now() + 5ms)
        .request(dummy, 1s)
        .receive(on_result, on_error);
      check_eq(*result, 9);
    }
  }
  SECTION("invalid response") {
    auto dummy = sys.spawn([]() -> behavior {
      return {
        [](int) { return "ok"s; },
      };
    });
    SECTION("regular message") {
      self->mail(3).delay(5ms).request(dummy, 1s).receive(on_result, on_error);
      check_eq(*err, make_error(sec::unexpected_response));
    }
    SECTION("urgent message") {
      auto [hdl, pending] = self->mail(3)
                              .urgent()
                              .schedule(self->clock().now() + 5ms)
                              .request(dummy, 1s);
      static_assert(
        std::is_same_v<decltype(hdl), blocking_response_handle<message>>);
      static_assert(std::is_same_v<decltype(pending), disposable>);
      std::move(hdl).receive(on_result, on_error);
      check_eq(*err, make_error(sec::unexpected_response));
    }
    SECTION("no response") {
      auto dummy = sys.spawn([](event_based_actor* self) -> behavior {
        auto res = std::make_shared<response_promise>();
        return {
          [self, res](int) { *res = self->make_response_promise(); },
        };
      });
      self->mail(3)
        .delay(5ms)
        .request(dummy, 10ms)
        .receive(on_result, on_error);
      check_eq(*err, make_error(sec::request_timeout));
      self->mail(exit_msg{self->address(), exit_reason::user_shutdown})
        .send(dummy);
    }
  }
}

TEST("send request message to an invalid receiver") {
  scoped_actor self{sys};
  auto result = std::make_shared<int>(0);
  auto on_result = [result](int x) { *result = x; };
  auto err = std::make_shared<error>();
  auto on_error = [err](error x) { *err = x; };
  SECTION("regular message") {
    self->mail("hello world").request(actor{}, 1s).receive(on_result, on_error);
    check_eq(*result, 0);
    check_eq(*err, make_error(sec::invalid_request));
  }
  SECTION("delayed message") {
    self->mail("hello world")
      .delay(1s)
      .request(actor{}, 1s)
      .receive(on_result, on_error);
    check_eq(*result, 0);
    check_eq(*err, make_error(sec::invalid_request));
  }
}

TEST("receive response as an expected") {
  using server_actor
    = typed_actor<result<void>(get_atom),                // No response.
                  result<int>(get_atom, int),            // One response value.
                  result<int, int>(get_atom, int, int)>; // Two response values.
  scoped_actor self{sys};
  SECTION("receive without delay") {
    SECTION("statically typed response with well-behaved server") {
      auto server = sys.spawn([]() -> server_actor::behavior_type {
        return {
          [](get_atom) {},
          [](get_atom, int x) { return x; },
          [](get_atom, int x, int y) -> result<int, int> {
            return {x, y};
          },
        };
      });
      SECTION("empty result") {
        auto res = self->mail(get_atom_v).request(server, 1s).receive();
        check(res.has_value());
      }
      SECTION("result with one integer") {
        auto res = self->mail(get_atom_v, 1).request(server, 1s).receive();
        check_eq(res, 1);
      }
      SECTION("result with two integers") {
        auto res = self->mail(get_atom_v, 1, 2).request(server, 1s).receive();
        check_eq(res, std::tuple{1, 2});
      }
    }
    SECTION("dynamically typed response with well-behaved server") {
      auto server = sys.spawn([]() -> behavior {
        return {
          [](get_atom) {},
          [](get_atom, int x) { return x; },
          [](get_atom, int x, int y) -> result<int, int> {
            return {x, y};
          },
        };
      });
      SECTION("empty result") {
        auto res = self->mail(get_atom_v).request(server, 1s).receive<>();
        check(res.has_value());
      }
      SECTION("result with one integer") {
        auto res = self->mail(get_atom_v, 1).request(server, 1s).receive<int>();
        check_eq(res, 1);
      }
      SECTION("result with two integers") {
        auto res = self->mail(get_atom_v, 1, 2)
                     .request(server, 1s)
                     .receive<int, int>();
        check_eq(res, std::tuple{1, 2});
      }
    }
    SECTION("statically typed response with a server returning errors") {
      auto server = sys.spawn([]() -> server_actor::behavior_type {
        return {
          [](get_atom) -> result<void> {
            return make_error(sec::runtime_error);
          },
          [](get_atom, int) -> result<int> {
            return make_error(sec::runtime_error);
          },
          [](get_atom, int, int) -> result<int, int> {
            return make_error(sec::runtime_error);
          },
        };
      });
      SECTION("empty result") {
        auto res = self->mail(get_atom_v).request(server, 1s).receive();
        check_eq(res, make_error(sec::runtime_error));
      }
      SECTION("result with one integer") {
        auto res = self->mail(get_atom_v, 1).request(server, 1s).receive();
        check_eq(res, make_error(sec::runtime_error));
      }
      SECTION("result with two integers") {
        auto res = self->mail(get_atom_v, 1, 2).request(server, 1s).receive();
        check_eq(res, make_error(sec::runtime_error));
      }
    }
    SECTION("dynamically typed response with well-behaved server") {
      auto server = sys.spawn([]() -> behavior {
        return {
          [](get_atom) -> result<void> {
            return make_error(sec::runtime_error);
          },
          [](get_atom, int) -> result<int> {
            return make_error(sec::runtime_error);
          },
          [](get_atom, int, int) -> result<int, int> {
            return make_error(sec::runtime_error);
          },
        };
      });
      SECTION("empty result") {
        auto res = self->mail(get_atom_v).request(server, 1s).receive<>();
        check_eq(res, make_error(sec::runtime_error));
      }
      SECTION("result with one integer") {
        auto res = self->mail(get_atom_v, 1).request(server, 1s).receive<int>();
        check_eq(res, make_error(sec::runtime_error));
      }
      SECTION("result with two integers") {
        auto res = self->mail(get_atom_v, 1, 2)
                     .request(server, 1s)
                     .receive<int, int>();
        check_eq(res, make_error(sec::runtime_error));
      }
    }
  }
  SECTION("receive with delay") {
    SECTION("statically typed response with well-behaved server") {
      auto server = sys.spawn([]() -> server_actor::behavior_type {
        return {
          [](get_atom) {},
          [](get_atom, int x) { return x; },
          [](get_atom, int x, int y) -> result<int, int> {
            return {x, y};
          },
        };
      });
      SECTION("empty result") {
        auto res
          = self->mail(get_atom_v).delay(10us).request(server, 1s).receive();
        check(res.has_value());
      }
      SECTION("result with one integer") {
        auto res
          = self->mail(get_atom_v, 1).delay(10us).request(server, 1s).receive();
        check_eq(res, 1);
      }
      SECTION("result with two integers") {
        auto res = self->mail(get_atom_v, 1, 2)
                     .delay(10us)
                     .request(server, 1s)
                     .receive();
        check_eq(res, std::tuple{1, 2});
      }
    }
    SECTION("dynamically typed response with well-behaved server") {
      auto server = sys.spawn([]() -> behavior {
        return {
          [](get_atom) {},
          [](get_atom, int x) { return x; },
          [](get_atom, int x, int y) -> result<int, int> {
            return {x, y};
          },
        };
      });
      SECTION("empty result") {
        auto res
          = self->mail(get_atom_v).delay(10us).request(server, 1s).receive<>();
        check(res.has_value());
      }
      SECTION("result with one integer") {
        auto res = self->mail(get_atom_v, 1)
                     .delay(10us)
                     .request(server, 1s)
                     .receive<int>();
        check_eq(res, 1);
      }
      SECTION("result with two integers") {
        auto res = self->mail(get_atom_v, 1, 2)
                     .delay(10us)
                     .request(server, 1s)
                     .receive<int, int>();
        check_eq(res, std::tuple{1, 2});
      }
    }
    SECTION("statically typed response with a server returning errors") {
      auto server = sys.spawn([]() -> server_actor::behavior_type {
        return {
          [](get_atom) -> result<void> {
            return make_error(sec::runtime_error);
          },
          [](get_atom, int) -> result<int> {
            return make_error(sec::runtime_error);
          },
          [](get_atom, int, int) -> result<int, int> {
            return make_error(sec::runtime_error);
          },
        };
      });
      SECTION("empty result") {
        auto res
          = self->mail(get_atom_v).delay(10us).request(server, 1s).receive();
        check_eq(res, make_error(sec::runtime_error));
      }
      SECTION("result with one integer") {
        auto res
          = self->mail(get_atom_v, 1).delay(10us).request(server, 1s).receive();
        check_eq(res, make_error(sec::runtime_error));
      }
      SECTION("result with two integers") {
        auto res = self->mail(get_atom_v, 1, 2)
                     .delay(10us)
                     .request(server, 1s)
                     .receive();
        check_eq(res, make_error(sec::runtime_error));
      }
    }
    SECTION("dynamically typed response with well-behaved server") {
      auto server = sys.spawn([]() -> behavior {
        return {
          [](get_atom) -> result<void> {
            return make_error(sec::runtime_error);
          },
          [](get_atom, int) -> result<int> {
            return make_error(sec::runtime_error);
          },
          [](get_atom, int, int) -> result<int, int> {
            return make_error(sec::runtime_error);
          },
        };
      });
      SECTION("empty result") {
        auto res
          = self->mail(get_atom_v).delay(10us).request(server, 1s).receive<>();
        check_eq(res, make_error(sec::runtime_error));
      }
      SECTION("result with one integer") {
        auto res = self->mail(get_atom_v, 1)
                     .delay(10us)
                     .request(server, 1s)
                     .receive<int>();
        check_eq(res, make_error(sec::runtime_error));
      }
      SECTION("result with two integers") {
        auto res = self->mail(get_atom_v, 1, 2)
                     .delay(10us)
                     .request(server, 1s)
                     .receive<int, int>();
        check_eq(res, make_error(sec::runtime_error));
      }
    }
  }
}

} // WITH_FIXTURE(fixture)

} // namespace
