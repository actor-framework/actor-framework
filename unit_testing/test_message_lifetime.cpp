#include <atomic>
#include <iostream>

#include "test.hpp"

#include "caf/all.hpp"

using std::cout;
using std::endl;
using namespace caf;

class testee : public event_based_actor {
 public:
  testee();
  ~testee();
  behavior make_behavior() override;
};

testee::testee() {
  // nop
}

testee::~testee() {
  // nop
}

behavior testee::make_behavior() {
  return {
    others() >> [=] {
      CAF_CHECK_EQUAL(last_dequeued().cvals()->get_reference_count(), 2);
      quit();
      return std::move(last_dequeued());
    }
  };
}

class tester : public event_based_actor {
 public:
  tester(actor aut);
  ~tester();
  behavior make_behavior() override;
 private:
  actor m_aut;
  message m_msg;
};

tester::tester(actor aut) :
    m_aut(std::move(aut)),
    m_msg(make_message(1, 2, 3)) {
  // nop
}

tester::~tester() {
  // nop
}

behavior tester::make_behavior() {
  monitor(m_aut);
  send(m_aut, m_msg);
  return {
    on(1, 2, 3) >> [=] {
      CAF_CHECK_EQUAL(last_dequeued().cvals()->get_reference_count(), 2);
      CAF_CHECK(last_dequeued().cvals().get() == m_msg.cvals().get());
    },
    [=](const down_msg& dm) {
      CAF_CHECK(dm.source == m_aut);
      CAF_CHECK_EQUAL(dm.reason, exit_reason::normal);
      CAF_CHECK_EQUAL(last_dequeued().cvals()->get_reference_count(), 1);
      quit();
    },
    others() >> CAF_UNEXPECTED_MSG_CB(this)
  };
}

void test_message_lifetime_in_scoped_actor() {
  auto msg = make_message(1, 2, 3);
  scoped_actor self;
  self->send(self, msg);
  self->receive(
    on(1, 2, 3) >> [&] {
      CAF_CHECK_EQUAL(msg.cvals()->get_reference_count(), 2);
      CAF_CHECK_EQUAL(self->last_dequeued().cvals()->get_reference_count(), 2);
      CAF_CHECK(self->last_dequeued().cvals().get() == msg.cvals().get());
    }
  );
  CAF_CHECK_EQUAL(msg.cvals()->get_reference_count(), 1);
  msg = make_message(42);
  self->send(self, msg);
  self->receive(
    [&](int& value) {
      CAF_CHECK_EQUAL(msg.cvals()->get_reference_count(), 1);
      CAF_CHECK_EQUAL(self->last_dequeued().cvals()->get_reference_count(), 1);
      CAF_CHECK(self->last_dequeued().cvals().get() != msg.cvals().get());
      value = 10;
    }
  );
  CAF_CHECK_EQUAL(msg.get_as<int>(0), 42);
}

template <spawn_options Os>
void test_message_lifetime() {
  test_message_lifetime_in_scoped_actor();
  if (CAF_TEST_RESULT() != 0) {
    return;
  }
  // put some preassure on the scheduler (check for thread safety)
  for (size_t i = 0; i < 100; ++i) {
    spawn<tester>(spawn<testee, Os>());
  }
}

int main() {
  CAF_TEST(test_message_lifetime);
  CAF_PRINT("test_message_lifetime<no_spawn_options>");
  test_message_lifetime<no_spawn_options>();
  await_all_actors_done();
  CAF_PRINT("test_message_lifetime<priority_aware>");
  test_message_lifetime<priority_aware>();
  await_all_actors_done();
  shutdown();
  return CAF_TEST_RESULT();
}
