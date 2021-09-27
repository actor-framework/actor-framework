// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

// This test suite checks the testing DSL itself.

#define CAF_SUITE dsl

#include "core-test.hpp"

using namespace caf;

namespace {

struct testee_state {
  behavior make_behavior() {
    return {
      [](add_atom, int32_t x, int32_t y) { return x + y; },
      [](sub_atom, int32_t x, int32_t y) { return x - y; },
    };
  }
};

using testee_actor = stateful_actor<testee_state>;

struct bool_list {
  std::vector<bool> values;
  auto begin() {
    return values.begin();
  }
  auto end() {
    return values.end();
  }
};

bool_list& operator<<(bool_list& ls, bool value) {
  ls.values.push_back(value);
  return ls;
}

} // namespace

BEGIN_FIXTURE_SCOPE(test_coordinator_fixture<>)

SCENARIO("successful checks increment the 'good' counter") {
  GIVEN("a unit test") {
    auto this_test = test::engine::current_test();
    WHEN("using any CHECK macro with a true statement") {
      THEN("the 'good' counter increments by one per check") {
        bool_list results;
        results << CHECK_EQ(this_test->good(), 0u);
        results << CHECK_EQ(this_test->good(), 1u);
        results << CHECK_EQ(this_test->good(), 2u);
        results << CHECK(true);
        results << CHECK_NE(1, 2);
        results << CHECK_LT(1, 2);
        results << CHECK_LE(1, 2);
        results << CHECK_GT(2, 1);
        results << CHECK_GE(2, 1);
        results << CHECK_EQ(this_test->good(), 9u);
        results << CHECK_EQ(this_test->bad(), 0u);
        auto is_true = [](bool x) { return x; };
        CHECK(std::all_of(results.begin(), results.end(), is_true));
      }
    }
  }
}

SCENARIO("unsuccessful checks increment the 'bad' counter") {
  GIVEN("a unit test") {
    auto this_test = test::engine::current_test();
    WHEN("using any CHECK macro with a false statement") {
      THEN("the 'bad' counter increments by one per check") {
        // Run the known-to-fail tests with suppressed output.
        auto bk = caf::test::logger::instance().make_quiet();
        bool_list results;
        results << CHECK_EQ(this_test->good(), 1u);
        results << CHECK(false);
        results << CHECK_NE(1, 1);
        results << CHECK_LT(2, 1);
        results << CHECK_LE(2, 1);
        results << CHECK_GT(1, 2);
        results << CHECK_GE(1, 2);
        caf::test::logger::instance().levels(bk);
        auto failed_checks = this_test->bad();
        this_test->reset(); // Prevent the unit test from actually failing.
        CHECK_EQ(failed_checks, 7u);
        REQUIRE_EQ(results.values.size(), 7u);
        auto is_false = [](bool x) { return !x; };
        CHECK(std::all_of(results.begin(), results.end(), is_false));
      }
    }
  }
}

SCENARIO("the test scheduler allows manipulating the control flow") {
  GIVEN("some event-based  actors") {
    auto aut1 = sys.spawn<testee_actor>();
    auto aut2 = sys.spawn<testee_actor>();
    auto aut3 = sys.spawn<testee_actor>();
    run();
    WHEN("sending messages to event-based actors") {
      THEN("the actors become jobs in the scheduler") {
        CHECK(!sched.has_job());
        self->send(aut1, add_atom_v, 1, 2);
        CHECK(sched.has_job());
        CHECK_EQ(sched.jobs.size(), 1u);
        self->send(aut2, add_atom_v, 2, 3);
        CHECK_EQ(sched.jobs.size(), 2u);
        self->send(aut3, add_atom_v, 3, 4);
        CHECK_EQ(sched.jobs.size(), 3u);
      }
      AND("prioritize allows moving actors to the head of the job queue") {
        auto ptr = [](const actor& hdl) {
          auto actor_ptr = actor_cast<caf::abstract_actor*>(hdl);
          return dynamic_cast<resumable*>(actor_ptr);
        };
        CHECK_EQ(&sched.next_job(), ptr(aut1));
        CHECK(sched.prioritize(aut2));
        CHECK_EQ(&sched.next_job(), ptr(aut2));
        CHECK(sched.prioritize(aut3));
        CHECK_EQ(&sched.next_job(), ptr(aut3));
        CHECK(sched.prioritize(aut1));
        CHECK_EQ(&sched.next_job(), ptr(aut1));
      }
      AND("peek allows inspecting the mailbox of the next job") {
        using std::make_tuple;
        auto peek = [this] { return sched.peek<add_atom, int, int>(); };
        CHECK_EQ(peek(), make_tuple(add_atom_v, 1, 2));
        CHECK(sched.prioritize(aut2));
        CHECK_EQ(peek(), make_tuple(add_atom_v, 2, 3));
        CHECK(sched.prioritize(aut3));
        CHECK_EQ(peek(), make_tuple(add_atom_v, 3, 4));
      }
      AND("run_until and run_once allows executing jobs selectively") {
        CHECK_EQ(run_until([this] { return sched.jobs.size() == 1; }), 2u);
        CHECK(run_once());
        CHECK(!sched.has_job());
        CHECK(!run_once());
      }
    }
  }
}

SCENARIO("allow() turns into a no-op on mismatch") {
  GIVEN("an event-based actor") {
    auto aut = sys.spawn<testee_actor>();
    run();
    WHEN("allow()-ing a message if no message is waiting in any mailbox") {
      THEN("allow() becomes a no-op and returns false") {
        CHECK(!(allow((add_atom, int32_t, int32_t), from(self).to(aut))));
        CHECK(!(allow((add_atom, int32_t, int32_t),
                      from(self).to(aut).with(_, _, _))));
      }
    }
    WHEN("allow()-ing a message but a different message is waiting") {
      THEN("allow() becomes a no-op and returns false") {
        self->send(aut, sub_atom_v, 4, 3);
        auto fake_sender = sys.spawn<testee_actor>();
        // Wrong type.
        CHECK(!(allow((add_atom, int32_t, int32_t), from(self).to(aut))));
        // Wrong type plus .with() check.
        CHECK(!(allow((add_atom, int32_t, int32_t),
                      from(self).to(aut).with(_, 4, 3))));
        // Correct type but .with() check expects different values.
        CHECK(!(allow((sub_atom, int32_t, int32_t),
                      from(self).to(aut).with(_, 1, 2))));
        // Correct type and matching .with() but wrong sender.
        CHECK(!(allow((sub_atom, int32_t, int32_t),
                      from(fake_sender).to(aut).with(_, 4, 3))));
        // Message must still wait in the mailbox.
        auto& aut_dref = sched.next_job<testee_actor>();
        REQUIRE_EQ(actor_cast<abstract_actor*>(aut), &aut_dref);
        auto msg_ptr = aut_dref.peek_at_next_mailbox_element();
        REQUIRE_NE(msg_ptr, nullptr);
        CHECK(msg_ptr->payload.matches(sub_atom_v, 4, 3));
        // Drop test message.
        run();
        while (!self->mailbox().empty())
          self->dequeue();
      }
    }
    WHEN("allow()-ing and a matching message arrives") {
      THEN("the actor processes the message and allow() returns true") {
        self->send(aut, sub_atom_v, 4, 3);
        CHECK(sched.has_job());
        CHECK(allow((sub_atom, int32_t, int32_t),
                    from(self).to(aut).with(_, 4, 3)));
        CHECK(!sched.has_job());
        CHECK(allow((int32_t), from(aut).to(self).with(1)));
      }
    }
  }
}

#ifdef CAF_ENABLE_EXCEPTIONS

SCENARIO("tests may check for exceptions") {
  GIVEN("a unit test") {
    auto this_test = test::engine::current_test();
    WHEN("using any CHECK_THROWS macro with a matching exception") {
      THEN("the 'good' counter increments by one per check") {
        auto f = [] { throw std::runtime_error("foo"); };
        CHECK_THROWS_AS(f(), std::runtime_error);
        CHECK_THROWS_WITH(f(), "foo");
        CHECK_THROWS_WITH_AS(f(), "foo", std::runtime_error);
        CHECK_NOTHROW([] {}());
      }
    }
    WHEN("using any CHECK_THROWS macro with a unexpected exception") {
      THEN("the 'bad' counter increments by one per check") {
        auto f = [] { throw std::logic_error("bar"); };
        auto bk = caf::test::logger::instance().make_quiet();
        CHECK_THROWS_AS(f(), std::runtime_error);
        CHECK_THROWS_WITH(f(), "foo");
        CHECK_THROWS_WITH_AS(f(), "foo", std::runtime_error);
        CHECK_THROWS_WITH_AS(f(), "foo", std::logic_error);
        CHECK_NOTHROW(f());
        caf::test::logger::instance().levels(bk);
        CHECK_EQ(this_test->bad(), 5u);
        this_test->reset_bad();
      }
    }
  }
}

SCENARIO("passing requirements increment the 'good' counter") {
  GIVEN("a unit test") {
    auto this_test = test::engine::current_test();
    WHEN("using any REQUIRE macro with a true statement") {
      THEN("the 'good' counter increments by one per requirement") {
        REQUIRE_EQ(this_test->good(), 0u);
        REQUIRE_EQ(this_test->good(), 1u);
        REQUIRE_EQ(this_test->good(), 2u);
        REQUIRE(true);
        REQUIRE_NE(1, 2);
        REQUIRE_LT(1, 2);
        REQUIRE_LE(1, 2);
        REQUIRE_GT(2, 1);
        REQUIRE_GE(2, 1);
        REQUIRE_EQ(this_test->good(), 9u);
        REQUIRE_EQ(this_test->bad(), 0u);
      }
    }
  }
}

#  define CHECK_FAILS(expr)                                                    \
    if (true) {                                                                \
      auto silent_expr = [&] {                                                 \
        auto bk = caf::test::logger::instance().make_quiet();                  \
        auto guard = caf::detail::make_scope_guard(                            \
          [bk] { caf::test::logger::instance().levels(bk); });                 \
        expr;                                                                  \
      };                                                                       \
      CHECK_THROWS_AS(silent_expr(), test::requirement_error);                 \
    }                                                                          \
    if (CHECK_EQ(this_test->bad(), 1u))                                        \
    this_test->reset_bad()

SCENARIO("failing requirements increment the 'bad' counter and throw") {
  GIVEN("a unit test") {
    auto this_test = test::engine::current_test();
    WHEN("using any REQUIRE macro with a true statement") {
      THEN("the 'good' counter increments by one per requirement") {
        CHECK_FAILS(REQUIRE_EQ(1, 2));
        CHECK_FAILS(REQUIRE_EQ(this_test->good(), 42u));
        CHECK_FAILS(REQUIRE(false));
        CHECK_FAILS(REQUIRE_NE(1, 1));
        CHECK_FAILS(REQUIRE_LT(2, 1));
        CHECK_FAILS(REQUIRE_LE(2, 1));
        CHECK_FAILS(REQUIRE_GT(1, 2));
        CHECK_FAILS(REQUIRE_GE(1, 2));
      }
    }
  }
}

SCENARIO("disallow() throws when finding a prohibited message") {
  GIVEN("an event-based actor") {
    auto aut = sys.spawn<testee_actor>();
    run();
    WHEN("disallow()-ing a message if no message is waiting in any mailbox") {
      THEN("disallow() becomes a no-op") {
        CHECK_NOTHROW(disallow((add_atom, int32_t, int32_t), //
                               from(self).to(aut)));
        CHECK_NOTHROW(disallow((add_atom, int32_t, int32_t),
                               from(self).to(aut).with(_, _, _)));
      }
    }
    WHEN("disallow()-ing a message if no matching message exists") {
      THEN("disallow() becomes a no-op") {
        self->send(aut, sub_atom_v, 4, 3);
        auto fake_sender = sys.spawn<testee_actor>();
        CHECK_NOTHROW(disallow((add_atom, int32_t, int32_t), to(aut)));
        CHECK_NOTHROW(disallow((add_atom, int32_t, int32_t), //
                               to(aut).with(_, _, _)));
        CHECK_NOTHROW(disallow((add_atom, int32_t, int32_t), //
                               from(self).to(aut)));
        CHECK_NOTHROW(disallow((add_atom, int32_t, int32_t),
                               from(self).to(aut).with(_, _, _)));
        CHECK_NOTHROW(disallow((sub_atom, int32_t, int32_t),
                               from(self).to(aut).with(_, 1, 2)));
        CHECK_NOTHROW(disallow((sub_atom, int32_t, int32_t), //
                               from(fake_sender).to(aut)));
        CHECK_NOTHROW(disallow((sub_atom, int32_t, int32_t), //
                               from(fake_sender).to(aut).with(_, 4, 3)));
        // Drop test message.
        run();
        while (!self->mailbox().empty())
          self->dequeue();
      }
    }
    WHEN("disallow()-ing an existing message") {
      THEN("disallow() throws and increment the 'bad' counter") {
        auto this_test = test::engine::current_test();
        self->send(aut, sub_atom_v, 4, 3);
        CHECK_FAILS(disallow((sub_atom, int32_t, int32_t), to(aut)));
        CHECK_FAILS(disallow((sub_atom, int32_t, int32_t), //
                             to(aut).with(_, _, _)));
        CHECK_FAILS(disallow((sub_atom, int32_t, int32_t), from(self).to(aut)));
        CHECK_FAILS(disallow((sub_atom, int32_t, int32_t), //
                             from(self).to(aut).with(_, _, _)));
      }
    }
  }
}

SCENARIO("expect() throws when not finding the required message") {
  GIVEN("an event-based actor") {
    auto this_test = test::engine::current_test();
    auto aut = sys.spawn<testee_actor>();
    run();
    WHEN("expect()-ing a message if no message is waiting in any mailbox") {
      THEN("expect() throws and increment the 'bad' counter") {
        CHECK_FAILS(expect((add_atom, int32_t, int32_t), to(aut)));
        CHECK_FAILS(expect((add_atom, int32_t, int32_t), //
                           to(aut).with(_, _, _)));
        CHECK_FAILS(expect((add_atom, int32_t, int32_t), //
                           from(self).to(aut)));
        CHECK_FAILS(expect((add_atom, int32_t, int32_t),
                           from(self).to(aut).with(_, _, _)));
        auto aut2 = sys.spawn<testee_actor>();
        CHECK_FAILS(expect((add_atom, int32_t, int32_t), to(aut2)));
        CHECK_FAILS(expect((add_atom, int32_t, int32_t), //
                           to(aut2).with(_, _, _)));
        CHECK_FAILS(expect((add_atom, int32_t, int32_t), //
                           from(self).to(aut2)));
        CHECK_FAILS(expect((add_atom, int32_t, int32_t),
                           from(self).to(aut2).with(_, _, _)));
      }
    }
    WHEN("expect()-ing a message if no matching message exists") {
      THEN("expect() throws and increment the 'bad' counter") {
        self->send(aut, sub_atom_v, 4, 3);
        auto fake_sender = sys.spawn<testee_actor>();
        CHECK_FAILS(expect((add_atom, int32_t, int32_t), to(aut)));
        CHECK_FAILS(expect((add_atom, int32_t, int32_t), //
                           to(aut).with(_, _, _)));
        CHECK_FAILS(expect((add_atom, int32_t, int32_t), //
                           from(self).to(aut)));
        CHECK_FAILS(expect((add_atom, int32_t, int32_t),
                           from(self).to(aut).with(_, _, _)));
        CHECK_FAILS(expect((sub_atom, int32_t, int32_t),
                           from(self).to(aut).with(_, 1, 2)));
        CHECK_FAILS(expect((sub_atom, int32_t, int32_t), //
                           from(fake_sender).to(aut)));
        CHECK_FAILS(expect((sub_atom, int32_t, int32_t), //
                           from(fake_sender).to(aut).with(_, 4, 3)));
        // Drop test message.
        run();
        while (!self->mailbox().empty())
          self->dequeue();
      }
    }
    WHEN("expect()-ing an existing message") {
      THEN("expect() processes the message") {
        self->send(aut, add_atom_v, 4, 3);
        CHECK_NOTHROW(expect((add_atom, int32_t, int32_t), to(aut)));
        CHECK_NOTHROW(expect((int32_t), to(self)));
        self->send(aut, add_atom_v, 4, 3);
        CHECK_NOTHROW(expect((add_atom, int32_t, int32_t), //
                             to(aut).with(_, _, _)));
        CHECK_NOTHROW(expect((int32_t), to(self).with(7)));
        self->send(aut, add_atom_v, 4, 3);
        CHECK_NOTHROW(expect((add_atom, int32_t, int32_t), from(self).to(aut)));
        CHECK_NOTHROW(expect((int32_t), from(aut).to(self)));
        self->send(aut, add_atom_v, 4, 3);
        CHECK_NOTHROW(expect((add_atom, int32_t, int32_t), //
                             from(self).to(aut).with(_, _, _)));
        CHECK_NOTHROW(expect((int32_t), from(aut).to(self).with(7)));
      }
    }
  }
}

#endif

END_FIXTURE_SCOPE()
