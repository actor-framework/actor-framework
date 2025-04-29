// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/mail_cache.hpp"

#include "caf/detail/critical.hpp"
#include "caf/detail/sync_request_bouncer.hpp"
#include "caf/local_actor.hpp"
#include "caf/mailbox_element.hpp"

namespace caf {

mailbox_element* safe_current_mailbox_element(local_actor* self) {
  auto* ptr = self->current_mailbox_element();
  if (!ptr) {
#ifdef CAF_ENABLE_EXCEPTIONS
    throw std::runtime_error("mail cache: current element is null");
#else
    CAF_CRITICAL("mail cache: current element is null");
#endif
  }
  return ptr;
}

mail_cache::~mail_cache() {
  if (!empty()) {
    detail::sync_request_bouncer bouncer{make_error(sec::disposed)};
    while (auto raw = elements_.pop()) {
      auto ptr = mailbox_element_ptr{raw};
      bouncer(*ptr);
    }
  }
}

void mail_cache::stash(message msg) {
  return do_stash(safe_current_mailbox_element(self_), std::move(msg));
}

void mail_cache::do_stash_current() {
  auto* ptr = safe_current_mailbox_element(self_);
  return do_stash(ptr, std::move(ptr->payload));
}

void mail_cache::do_stash(mailbox_element* src, message&& msg) {
  if (size_ == max_size_) {
#ifdef CAF_ENABLE_EXCEPTIONS
    throw std::runtime_error("mail cache exceeded its maximum size");
#else
    CAF_CRITICAL("mail cache exceeded its maximum size");
#endif
  }
  auto element = make_mailbox_element(src->sender, src->mid, std::move(msg));
  src->mid.mark_as_answered();
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
