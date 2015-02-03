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
      CAF_CHECK_EQUAL(last_dequeued().vals()->get_reference_count(), 2);
      quit();
      return last_dequeued();
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
      CAF_CHECK(last_dequeued().vals()->get_reference_count() >= 2);
      CAF_CHECK(last_dequeued().vals().get() == m_msg.vals().get());
    },
    [=](const down_msg& dm) {
      CAF_CHECK(dm.source == m_aut);
      CAF_CHECK_EQUAL(dm.reason, exit_reason::normal);
      CAF_CHECK_EQUAL(last_dequeued().vals()->get_reference_count(), 1);
      quit();
    },
    others() >> CAF_UNEXPECTED_MSG_CB(this)
  };
}

void test_message_lifetime() {
  // put some preassure on the scheduler
  for (size_t i = 0; i < 1000; ++i) {
    spawn<tester>(spawn<testee>());
  }
}

int main() {
  CAF_TEST(test_message_lifetime);
  test_message_lifetime();
  await_all_actors_done();
  shutdown();
  return CAF_TEST_RESULT();
}
