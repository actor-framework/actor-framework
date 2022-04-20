// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE policy.categorized

#include "caf/policy/categorized.hpp"

#include "core-test.hpp"

#include "caf/intrusive/drr_queue.hpp"
#include "caf/intrusive/fifo_inbox.hpp"
#include "caf/intrusive/wdrr_dynamic_multiplexed_queue.hpp"
#include "caf/intrusive/wdrr_fixed_multiplexed_queue.hpp"
#include "caf/policy/normal_messages.hpp"
#include "caf/policy/urgent_messages.hpp"
#include "caf/unit.hpp"

using namespace caf;

namespace {

using urgent_queue = intrusive::drr_queue<policy::urgent_messages>;

using normal_queue = intrusive::drr_queue<policy::normal_messages>;

struct mailbox_policy {
  using deficit_type = size_t;

  using mapped_type = mailbox_element;

  using unique_pointer = mailbox_element_ptr;

  using queue_type
    = intrusive::wdrr_fixed_multiplexed_queue<policy::categorized, urgent_queue,
                                              normal_queue>;
};

using mailbox_type = intrusive::fifo_inbox<mailbox_policy>;

struct fixture {};

struct consumer {
  std::vector<int> ints;

  template <class Key, class Queue>
  intrusive::task_result
  operator()(const Key&, const Queue&, const mailbox_element& x) {
    if (!x.content().match_elements<int>())
      CAF_FAIL("unexpected message: " << x.content());
    ints.emplace_back(x.content().get_as<int>(0));
    return intrusive::task_result::resume;
  }

  template <class Key, class Queue, class... Ts>
  intrusive::task_result operator()(const Key&, const Queue&, const Ts&...) {
    CAF_FAIL("unexpected message type"); // << typeid(Ts).name());
    return intrusive::task_result::resume;
  }
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

CAF_TEST(priorities) {
  mailbox_type mbox{unit, unit, unit};
  mbox.push_back(make_mailbox_element(nullptr, make_message_id(), {}, 123));
  mbox.push_back(make_mailbox_element(
    nullptr, make_message_id(message_priority::high), {}, 456));
  consumer f;
  mbox.new_round(1000, f);
  CHECK_EQ(f.ints, std::vector<int>({456, 123}));
}

END_FIXTURE_SCOPE()
