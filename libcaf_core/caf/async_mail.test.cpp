// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/async_mail.hpp"

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/test.hpp"

#include "caf/actor_traits.hpp"
#include "caf/behavior.hpp"
#include "caf/dynamically_typed.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/message_priority.hpp"

using namespace caf;
using namespace std::literals;

namespace {

class testee : public event_based_actor {
public:
  using super = event_based_actor;

  using super::super;

  template <class... Args>
  auto mail(Args&&... args) {
    return async_mail(dynamically_typed{}, this, std::forward<Args>(args)...);
  }
};

WITH_FIXTURE(test::fixture::deterministic) {

TEST("send asynchronous message") {
  auto [self, launch] = sys.spawn_inactive<testee>();
  auto dummy = sys.spawn([](event_based_actor*) -> behavior {
    return {
      [=](const std::string&) {},
    };
  });
  SECTION("regular message") {
    self->mail("hello world").send(dummy);
    expect<std::string>()
      .with("hello world")
      .priority(message_priority::normal)
      .from(self)
      .to(dummy);
  }
  SECTION("urgent message") {
    self->mail("hello world").urgent().send(dummy);
    expect<std::string>()
      .with("hello world")
      .priority(message_priority::high)
      .from(self)
      .to(dummy);
  }
}

TEST("delegate message") {
  SECTION("asynchronous message") {
    auto [self, launch] = sys.spawn_inactive<testee>();
    auto delegatee = sys.spawn([](event_based_actor*) -> behavior {
      return {
        [=](const std::string&) {},
      };
    });
    SECTION("delegate with default priority") {
      auto delegator = sys.spawn([delegatee](testee* self) -> behavior {
        return {
          [=](std::string& str) {
            return self->mail(std::move(str)).delegate(delegatee);
          },
        };
      });
      SECTION("regular message") {
        self->mail("hello world").send(delegator);
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::normal)
          .from(self)
          .to(delegator);
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::normal)
          .from(self)
          .to(delegatee);
      }
      SECTION("urgent message") {
        self->mail("hello world").urgent().send(delegator);
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::high)
          .from(self)
          .to(delegator);
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::high)
          .from(self)
          .to(delegatee);
      }
    }
    SECTION("delegate with high priority") {
      auto delegator = sys.spawn([delegatee](testee* self) -> behavior {
        return {
          [=](std::string& str) {
            return self->mail(std::move(str)).urgent().delegate(delegatee);
          },
        };
      });
      SECTION("regular message") {
        self->mail("hello world").send(delegator);
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::normal)
          .from(self)
          .to(delegator);
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::high)
          .from(self)
          .to(delegatee);
      }
      SECTION("urgent message") {
        self->mail("hello world").urgent().send(delegator);
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::high)
          .from(self)
          .to(delegator);
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::high)
          .from(self)
          .to(delegatee);
      }
    }
  }
  SECTION("request message") {
    auto [self, launch] = sys.spawn_inactive<testee>();
    SECTION("delegate with default priority") {
      auto delegatee = sys.spawn([](event_based_actor*) -> behavior {
        return {
          [=](const std::string& str) {
            return std::string{str.rbegin(), str.rend()};
          },
        };
      });
      auto delegator = sys.spawn([delegatee](testee* self) -> behavior {
        return {
          [=](std::string& str) {
            return self->mail(std::move(str)).delegate(delegatee);
          },
        };
      });
      SECTION("regular message") {
        self->request(delegator, infinite, "hello world")
          .then([](const std::string&) {});
        auto self_hdl = actor_cast<actor>(self);
        launch();
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::normal)
          .from(self_hdl)
          .to(delegator);
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::normal)
          .from(self_hdl)
          .to(delegatee);
        expect<std::string>()
          .with("dlrow olleh")
          .priority(message_priority::normal)
          .from(delegatee)
          .to(self_hdl);
      }
      SECTION("urgent message") {
        self
          ->request<message_priority::high>(delegator, infinite, "hello world")
          .then([](const std::string&) {});
        auto self_hdl = actor_cast<actor>(self);
        launch();
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::high)
          .from(self_hdl)
          .to(delegator);
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::high)
          .from(self_hdl)
          .to(delegatee);
        expect<std::string>()
          .with("dlrow olleh")
          .priority(message_priority::high)
          .from(delegatee)
          .to(self_hdl);
      }
    }
    SECTION("delegate with high priority") {
      auto delegatee = sys.spawn([](event_based_actor*) -> behavior {
        return {
          [=](const std::string&) {},
        };
      });
      auto delegator = sys.spawn([delegatee](testee* self) -> behavior {
        return {
          [=](std::string& str) {
            return self->mail(std::move(str)).urgent().delegate(delegatee);
          },
        };
      });
      SECTION("regular message") {
        self->mail("hello world").send(delegator);
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::normal)
          .from(self)
          .to(delegator);
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::high)
          .from(self)
          .to(delegatee);
      }
      SECTION("urgent message") {
        self->mail("hello world").urgent().send(delegator);
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::high)
          .from(self)
          .to(delegator);
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::high)
          .from(self)
          .to(delegatee);
      }
    }
  }
}

TEST("send delayed message") {
  auto [self, launch] = sys.spawn_inactive<testee>();
  auto dummy = sys.spawn([](event_based_actor*) -> behavior {
    return {
      [=](const std::string&) {},
    };
  });
  SECTION("regular message") {
    SECTION("strong reference to the sender") {
      SECTION("strong reference to the receiver") {
        self->mail("hello world")
          .delay(1s)
          .send(dummy, strong_ref, strong_self_ref);
        check_eq(mail_count(), 0u);
        check_eq(num_timeouts(), 1u);
        trigger_timeout();
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::normal)
          .from(self)
          .to(dummy);
      }
      SECTION("weak reference to the receiver") {
        self->mail("hello world")
          .delay(1s)
          .send(dummy, weak_ref, strong_self_ref);
        check_eq(mail_count(), 0u);
        check_eq(num_timeouts(), 1u);
        trigger_timeout();
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::normal)
          .from(self)
          .to(dummy);
      }
    }
    SECTION("weak reference to the sender") {
      SECTION("strong reference to the receiver") {
        self->mail("hello world")
          .delay(1s)
          .send(dummy, strong_ref, weak_self_ref);
        check_eq(mail_count(), 0u);
        check_eq(num_timeouts(), 1u);
        trigger_timeout();
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::normal)
          .from(self)
          .to(dummy);
      }
      SECTION("weak reference to the receiver") {
        self->mail("hello world")
          .delay(1s)
          .send(dummy, weak_ref, weak_self_ref);
        check_eq(mail_count(), 0u);
        check_eq(num_timeouts(), 1u);
        trigger_timeout();
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::normal)
          .from(self)
          .to(dummy);
      }
    }
  }
  SECTION("urgent message") {
    SECTION("strong reference to the sender") {
      SECTION("strong reference to the receiver") {
        self->mail("hello world")
          .urgent()
          .delay(1s)
          .send(dummy, strong_ref, strong_self_ref);
        check_eq(mail_count(), 0u);
        check_eq(num_timeouts(), 1u);
        trigger_timeout();
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::high)
          .from(self)
          .to(dummy);
      }
      SECTION("weak reference to the receiver") {
        self->mail("hello world")
          .urgent()
          .delay(1s)
          .send(dummy, weak_ref, strong_self_ref);
        check_eq(mail_count(), 0u);
        check_eq(num_timeouts(), 1u);
        trigger_timeout();
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::high)
          .from(self)
          .to(dummy);
      }
    }
    SECTION("weak reference to the sender") {
      SECTION("strong reference to the receiver") {
        self->mail("hello world")
          .urgent()
          .delay(1s)
          .send(dummy, strong_ref, weak_self_ref);
        check_eq(mail_count(), 0u);
        check_eq(num_timeouts(), 1u);
        trigger_timeout();
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::high)
          .from(self)
          .to(dummy);
      }
      SECTION("weak reference to the receiver") {
        self->mail("hello world")
          .urgent()
          .delay(1s)
          .send(dummy, weak_ref, weak_self_ref);
        check_eq(mail_count(), 0u);
        check_eq(num_timeouts(), 1u);
        trigger_timeout();
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::high)
          .from(self)
          .to(dummy);
      }
    }
  }
}

TEST("delay delegate message") {
  auto [self, launch] = sys.spawn_inactive<testee>();
  auto delegatee = sys.spawn([](event_based_actor*) -> behavior {
    return {
      [=](const std::string&) {},
    };
  });
  SECTION("regular message") {
    SECTION("strong reference to the sender") {
      auto delegator = sys.spawn([delegatee](testee* self) -> behavior {
        return {
          [=](std::string& str) {
            return self->mail(std::move(str))
              .delay(1s)
              .delegate(delegatee, strong_ref)
              .first;
          },
        };
      });
      SECTION("strong reference to the receiver") {
        self->mail("hello world").send(delegator);
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::normal)
          .from(self)
          .to(delegator);
        check_eq(mail_count(), 0u);
        check_eq(num_timeouts(), 1u);
        trigger_timeout();
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::normal)
          .from(self)
          .to(delegatee);
      }
      SECTION("weak reference to the receiver") {
        auto delegator = sys.spawn([delegatee](testee* self) -> behavior {
          return {
            [=](std::string& str) {
              return self->mail(std::move(str))
                .delay(1s)
                .delegate(delegatee, weak_ref)
                .first;
            },
          };
        });
        self->mail("hello world").send(delegator);
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::normal)
          .from(self)
          .to(delegator);
        check_eq(mail_count(), 0u);
        check_eq(num_timeouts(), 1u);
        trigger_timeout();
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::normal)
          .from(self)
          .to(delegatee);
      }
    }
    SECTION("strong reference to the sender") {
      auto delegator = sys.spawn([delegatee](testee* self) -> behavior {
        return {
          [=](std::string& str) {
            return self->mail(std::move(str))
              .delay(1s)
              .delegate(delegatee, strong_ref, weak_self_ref)
              .first;
          },
        };
      });
      SECTION("strong reference to the receiver") {
        self->mail("hello world").send(delegator);
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::normal)
          .from(self)
          .to(delegator);
        check_eq(mail_count(), 0u);
        check_eq(num_timeouts(), 1u);
        trigger_timeout();
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::normal)
          .from(self)
          .to(delegatee);
      }
      SECTION("weak reference to the receiver") {
        auto delegator = sys.spawn([delegatee](testee* self) -> behavior {
          return {
            [=](std::string& str) {
              return self->mail(std::move(str))
                .delay(1s)
                .delegate(delegatee, weak_ref, weak_self_ref)
                .first;
            },
          };
        });
        self->mail("hello world").send(delegator);
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::normal)
          .from(self)
          .to(delegator);
        check_eq(mail_count(), 0u);
        check_eq(num_timeouts(), 1u);
        trigger_timeout();
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::normal)
          .from(self)
          .to(delegatee);
      }
    }
  }
  SECTION("urgent message") {
    SECTION("strong reference to the sender") {
      auto delegator = sys.spawn([delegatee](testee* self) -> behavior {
        return {
          [=](std::string& str) {
            return self->mail(std::move(str))
              .urgent()
              .delay(1s)
              .delegate(delegatee, strong_ref)
              .first;
          },
        };
      });
      SECTION("strong reference to the receiver") {
        self->mail("hello world").send(delegator);
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::normal)
          .from(self)
          .to(delegator);
        check_eq(mail_count(), 0u);
        check_eq(num_timeouts(), 1u);
        trigger_timeout();
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::high)
          .from(self)
          .to(delegatee);
      }
      SECTION("weak reference to the receiver") {
        auto delegator = sys.spawn([delegatee](testee* self) -> behavior {
          return {
            [=](std::string& str) {
              return self->mail(std::move(str))
                .urgent()
                .delay(1s)
                .delegate(delegatee, weak_ref)
                .first;
            },
          };
        });
        self->mail("hello world").send(delegator);
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::normal)
          .from(self)
          .to(delegator);
        check_eq(mail_count(), 0u);
        check_eq(num_timeouts(), 1u);
        trigger_timeout();
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::high)
          .from(self)
          .to(delegatee);
      }
    }
    SECTION("strong reference to the sender") {
      auto delegator = sys.spawn([delegatee](testee* self) -> behavior {
        return {
          [=](std::string& str) {
            return self->mail(std::move(str))
              .urgent()
              .delay(1s)
              .delegate(delegatee, strong_ref, weak_self_ref)
              .first;
          },
        };
      });
      SECTION("strong reference to the receiver") {
        self->mail("hello world").send(delegator);
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::normal)
          .from(self)
          .to(delegator);
        check_eq(mail_count(), 0u);
        check_eq(num_timeouts(), 1u);
        trigger_timeout();
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::high)
          .from(self)
          .to(delegatee);
      }
      SECTION("weak reference to the receiver") {
        auto delegator = sys.spawn([delegatee](testee* self) -> behavior {
          return {
            [=](std::string& str) {
              return self->mail(std::move(str))
                .urgent()
                .delay(1s)
                .delegate(delegatee, weak_ref, weak_self_ref)
                .first;
            },
          };
        });
        self->mail("hello world").send(delegator);
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::normal)
          .from(self)
          .to(delegator);
        check_eq(mail_count(), 0u);
        check_eq(num_timeouts(), 1u);
        trigger_timeout();
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::high)
          .from(self)
          .to(delegatee);
      }
    }
  }
}

TEST("implicit cancel of a delayed message") {
  auto [self, launch] = sys.spawn_inactive<testee>();
  auto dummy = sys.spawn([](event_based_actor*) -> behavior {
    return {
      [=](const std::string&) {},
    };
  });
  SECTION("canceling due to the sender going out of scope") {
    self->mail("hello world").delay(1s).send(dummy, strong_ref, weak_self_ref);
    launch();
    check_eq(mail_count(), 0u);
    check_eq(num_timeouts(), 1u);
    trigger_timeout();
    check_eq(mail_count(), 0u);
    check_eq(num_timeouts(), 0u);
  }
  SECTION("canceling due to the receiver going out of scope") {
    self->mail("hello world").delay(1s).send(dummy, weak_ref, strong_self_ref);
    dummy = nullptr;
    check_eq(mail_count(), 0u);
    check_eq(num_timeouts(), 1u);
    trigger_timeout();
    check_eq(mail_count(), 0u);
    check_eq(num_timeouts(), 0u);
  }
}

TEST("explicit cancel of a delayed message") {
  auto [self, launch] = sys.spawn_inactive<testee>();
  auto dummy = sys.spawn([](event_based_actor*) -> behavior {
    return {
      [=](const std::string&) {},
    };
  });
  SECTION("strong reference to the sender") {
    SECTION("strong reference to the receiver") {
      auto hdl = self->mail("hello world").delay(1s).send(dummy, strong_ref);
      check_eq(mail_count(), 0u);
      check_eq(num_timeouts(), 1u);
      hdl.dispose();
      trigger_timeout();
      check_eq(mail_count(), 0u);
      check_eq(num_timeouts(), 0u);
    }
    SECTION("weak reference to the receiver") {
      auto hdl = self->mail("hello world").delay(1s).send(dummy, weak_ref);
      check_eq(mail_count(), 0u);
      check_eq(num_timeouts(), 1u);
      hdl.dispose();
      trigger_timeout();
      check_eq(mail_count(), 0u);
      check_eq(num_timeouts(), 0u);
    }
  }
  SECTION("weak reference to the sender") {
    SECTION("strong reference to the receiver") {
      auto hdl = self->mail("hello world")
                   .delay(1s)
                   .send(dummy, strong_ref, weak_self_ref);
      check_eq(mail_count(), 0u);
      check_eq(num_timeouts(), 1u);
      hdl.dispose();
      trigger_timeout();
      check_eq(mail_count(), 0u);
      check_eq(num_timeouts(), 0u);
    }
    SECTION("weak reference to the receiver") {
      auto hdl = self->mail("hello world")
                   .delay(1s)
                   .send(dummy, weak_ref, weak_self_ref);
      check_eq(mail_count(), 0u);
      check_eq(num_timeouts(), 1u);
      hdl.dispose();
      trigger_timeout();
      check_eq(mail_count(), 0u);
      check_eq(num_timeouts(), 0u);
    }
  }
}

TEST("sending to a null handle is a no-op") {
  auto [self, launch] = sys.spawn_inactive<testee>();
  auto hdl = actor{};
  self->mail("hello world").send(hdl);
  check_eq(mail_count(), 0u);
  check_eq(num_timeouts(), 0u);
  self->mail("hello world").delay(1s).send(hdl, strong_ref);
  check_eq(mail_count(), 0u);
  check_eq(num_timeouts(), 0u);
  self->mail("hello world").delay(1s).send(hdl, weak_ref);
  check_eq(mail_count(), 0u);
  check_eq(num_timeouts(), 0u);
}

TEST("delegating to a null handle is an error") {
  auto delegator = sys.spawn([](testee* self) -> behavior {
    return {
      [=](std::string& str) {
        return self->mail(std::move(str))
          .delay(1s)
          .delegate(actor{}, strong_ref)
          .first;
      },
    };
  });
  SECTION("regular dispatch") {
    auto [self, launch] = sys.spawn_inactive<testee>();
    self->mail("hello world").send(delegator);
    self->become([](int) {});
    auto self_hdl = actor_cast<actor>(self);
    launch();
    check_eq(mail_count(), 1u);
    expect<std::string>().with("hello world").from(self_hdl).to(delegator);
    check_eq(mail_count(), 1u);
    expect<error>().from(delegator).to(self_hdl);
  }
  SECTION("delayed dispatch") {
    auto [self, launch] = sys.spawn_inactive<testee>();
    self->mail("hello world").send(delegator);
    self->become([](int) {});
    auto self_hdl = actor_cast<actor>(self);
    launch();
    check_eq(mail_count(), 1u);
    expect<std::string>().with("hello world").from(self_hdl).to(delegator);
    check_eq(mail_count(), 1u);
    expect<error>().from(delegator).to(self_hdl);
  }
}

} // WITH_FIXTURE(test::fixture::deterministic)

} // namespace
