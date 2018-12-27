/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#define CAF_SUITE categorized

#include "caf/policy/categorized.hpp"

#include "caf/test/dsl.hpp"

#include "caf/intrusive/drr_queue.hpp"
#include "caf/intrusive/fifo_inbox.hpp"
#include "caf/intrusive/wdrr_dynamic_multiplexed_queue.hpp"
#include "caf/intrusive/wdrr_fixed_multiplexed_queue.hpp"
#include "caf/policy/downstream_messages.hpp"
#include "caf/policy/normal_messages.hpp"
#include "caf/policy/upstream_messages.hpp"
#include "caf/policy/urgent_messages.hpp"
#include "caf/unit.hpp"

using namespace caf;

namespace {

using urgent_queue = intrusive::drr_queue<policy::urgent_messages>;

using normal_queue = intrusive::drr_queue<policy::normal_messages>;

using upstream_queue = intrusive::drr_queue<policy::upstream_messages>;

using downstream_queue = intrusive::wdrr_dynamic_multiplexed_queue<
  policy::downstream_messages>;

struct mailbox_policy {
  using deficit_type = size_t;

  using mapped_type = mailbox_element;

  using unique_pointer = mailbox_element_ptr;

  using queue_type = intrusive::wdrr_fixed_multiplexed_queue<
    policy::categorized, urgent_queue, normal_queue, upstream_queue,
    downstream_queue>;
};

using mailbox_type = intrusive::fifo_inbox<mailbox_policy>;

struct fixture{};

struct consumer {
  std::vector<int> ints;

  template <class Key, class Queue>
  intrusive::task_result operator()(const Key&, const Queue&,
                                    const mailbox_element& x) {
    if (!x.content().match_elements<int>())
      CAF_FAIL("unexepected message: " << x.content());
    ints.emplace_back(x.content().get_as<int>(0));
    return intrusive::task_result::resume;
  }

  template <class Key, class Queue, class... Ts>
  intrusive::task_result operator()(const Key&, const Queue&, const Ts&...) {
    CAF_FAIL("unexepected message type");// << typeid(Ts).name());
    return intrusive::task_result::resume;
  }
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(categorized_tests, fixture)

CAF_TEST(priorities) {
  mailbox_type mbox{unit, unit, unit, unit, unit};
  mbox.push_back(make_mailbox_element(nullptr, make_message_id(), {}, 123));
  mbox.push_back(make_mailbox_element(nullptr,
                                      make_message_id(message_priority::high),
                                      {}, 456));
  consumer f;
  mbox.new_round(1000, f);
  CAF_CHECK_EQUAL(f.ints, std::vector<int>({456, 123}));
}

CAF_TEST_FIXTURE_SCOPE_END()
