// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/mail_cache.hpp"

#include "caf/abstract_actor.hpp"
#include "caf/config.hpp"
#include "caf/detail/sync_request_bouncer.hpp"
#include "caf/local_actor.hpp"

namespace caf {

mail_cache::~mail_cache() {
  if (!empty()) {
    error reason;
    if (self_->getf(abstract_actor::is_terminated_flag
                    | abstract_actor::is_shutting_down_flag)) {
      reason = make_error(sec::request_receiver_down);
    } else {
      reason = make_error(sec::disposed);
    }
    detail::sync_request_bouncer bouncer{std::move(reason)};
    while (auto raw = elements_.pop()) {
      auto ptr = mailbox_element_ptr{raw};
      bouncer(*ptr);
    }
  }
}

void mail_cache::stash(message msg) {
#ifdef CAF_ENABLE_EXCEPTIONS
  if (size_ == max_size_)
    throw std::runtime_error("mail cache exceeded its maximum size");
#endif
  auto element = make_mailbox_element(self_->current_sender(),
                                      self_->take_current_message_id(),
                                      std::move(msg));
  ++size_;
  elements_.push(element.release());
}

void mail_cache::unstash() {
  size_ = 0;
  while (auto ptr = elements_.pop()) {
    self_->do_unstash(mailbox_element_ptr{ptr});
  }
}

} // namespace caf
