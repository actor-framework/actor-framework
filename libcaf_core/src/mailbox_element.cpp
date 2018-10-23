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

#include "caf/mailbox_element.hpp"

#include "caf/message_builder.hpp"
#include "caf/type_nr.hpp"

namespace caf {

namespace {

message_id dynamic_category_correction(const message& msg, message_id mid) {
  auto tt = msg.type_token();
  if (tt == make_type_token<downstream_msg>())
    return mailbox_category_corrector<downstream_msg>::apply(mid);
  if (tt == make_type_token<upstream_msg>())
    return mailbox_category_corrector<upstream_msg>::apply(mid);
  return mid;
}

/// Wraps a `message` into a mailbox element.
class mailbox_element_wrapper final : public mailbox_element {
public:
  mailbox_element_wrapper(strong_actor_ptr&& x0, message_id x1,
                          forwarding_stack&& x2, message&& x3)
      : mailbox_element(std::move(x0), dynamic_category_correction(x3, x1),
                        std::move(x2)),
        msg_(std::move(x3)) {
    /// Make sure that `content` can access the pointer safely.
    if (msg_.vals() == nullptr) {
      message_builder mb;
      msg_ = mb.to_message();
    }
  }

  type_erased_tuple& content() override {
    return msg_;
  }

  const type_erased_tuple& content() const override {
    return msg_.content();
  }

  message move_content_to_message() override {
    return std::move(msg_);
  }

  message copy_content_to_message() const override {
    return msg_;
  }

private:
  /// Stores the content of this mailbox element.
  message msg_;
};

} // namespace <anonymous>

mailbox_element::mailbox_element() {
  // nop
}

mailbox_element::mailbox_element(strong_actor_ptr&& x, message_id y,
                                 forwarding_stack&& z)
    :sender(std::move(x)),
      mid(y),
      stages(std::move(z)) {
  // nop
}

mailbox_element::~mailbox_element() {
  // nop
}

mailbox_element_ptr
make_mailbox_element(strong_actor_ptr sender, message_id id,
                     mailbox_element::forwarding_stack stages, message msg) {
  auto ptr = new mailbox_element_wrapper(std::move(sender), id,
                                         std::move(stages), std::move(msg));
  return mailbox_element_ptr{ptr};
}

} // namespace caf
