#include "test.hpp"

#include "caf/all.hpp"

using namespace caf;

void test_constructor_attach() {
  class testee : public event_based_actor {
   public:
    testee(actor buddy) : m_buddy(buddy) {
      attach_functor([=](uint32_t reason) {
        send(m_buddy, atom("done"), reason);
      });
    }
    behavior make_behavior() {
      return {
        on(atom("die")) >> [=] {
          quit(exit_reason::user_shutdown);
        }
      };
    }
   private:
    actor m_buddy;
  };
  class spawner : public event_based_actor {
   public:
    spawner() : m_downs(0) {
    }
    behavior make_behavior() {
      m_testee = spawn<testee, monitored>(this);
      return {
        [=](const down_msg& msg) {
          CAF_CHECK_EQUAL(msg.reason, exit_reason::user_shutdown);
          if (++m_downs == 2) {
            quit(msg.reason);
          }
        },
        on(atom("done"), arg_match) >> [=](uint32_t reason) {
          CAF_CHECK_EQUAL(reason, exit_reason::user_shutdown);
          if (++m_downs == 2) {
            quit(reason);
          }
        },
        others() >> [=] {
          forward_to(m_testee);
        }
      };
    }
   private:
    int m_downs;
    actor m_testee;
  };
  anon_send(spawn<spawner>(), atom("die"));
}

int main() {
  CAF_TEST(test_constructor_attach);
  test_constructor_attach();
  return CAF_TEST_RESULT();
}
