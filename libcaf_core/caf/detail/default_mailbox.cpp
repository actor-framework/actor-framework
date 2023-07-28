// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/default_mailbox.hpp"

#include "caf/detail/sync_request_bouncer.hpp"
#include "caf/error.hpp"
#include "caf/message_id.hpp"

namespace caf::detail {

intrusive::inbox_result default_mailbox::push_back(mailbox_element_ptr ptr) {
  return inbox_.push_front(ptr.release());
}

void default_mailbox::push_front(mailbox_element_ptr ptr) {
  if (ptr->mid.is_urgent_message())
    urgent_queue_.push_front(ptr.release());
  else
    normal_queue_.push_front(ptr.release());
}

mailbox_element* default_mailbox::peek(message_id id) {
  if (inbox_.closed() || inbox_.blocked()) {
    return nullptr;
  }
  fetch_more();
  if (id.is_async()) {
    if (!urgent_queue_.empty())
      return urgent_queue_.front();
    if (!normal_queue_.empty())
      return normal_queue_.front();
    return nullptr;
  }
  auto pred = [id](mailbox_element& x) { return x.mid == id; };
  if (auto result = urgent_queue_.find_if(pred))
    return result;
  return normal_queue_.find_if(pred);
}

mailbox_element_ptr default_mailbox::pop_front() {
  for (;;) {
    if (auto result = urgent_queue_.pop_front())
      return result;
    if (auto result = normal_queue_.pop_front())
      return result;
    if (!fetch_more())
      return nullptr;
  }
}

bool default_mailbox::closed() {
  return inbox_.closed();
}

bool default_mailbox::blocked() {
  return inbox_.blocked();
}

bool default_mailbox::try_block() {
  return cached() == 0 && inbox_.try_block();
}

size_t default_mailbox::close(const error& reason) {
  size_t result = 0;
  detail::sync_request_bouncer bounce{reason};
  auto bounce_and_count = [&bounce, &result](mailbox_element* ptr) {
    bounce(*ptr);
    delete ptr;
    ++result;
  };
  urgent_queue_.drain(bounce_and_count);
  normal_queue_.drain(bounce_and_count);
  inbox_.close(bounce_and_count);
  return result;
}

size_t default_mailbox::size() {
  fetch_more();
  return cached();
}

bool default_mailbox::fetch_more() {
  using node_type = intrusive::singly_linked<mailbox_element>;
  auto promote = [](node_type* ptr) {
    return static_cast<mailbox_element*>(ptr);
  };
  auto* head = static_cast<node_type*>(inbox_.take_head());
  if (head == nullptr)
    return false;
  do {
    auto next = head->next;
    auto phead = promote(head);
    if (phead->mid.is_urgent_message())
      urgent_queue_.lifo_append(phead);
    else
      normal_queue_.lifo_append(phead);
    head = next;
  } while (head != nullptr);
  urgent_queue_.stop_lifo_append();
  normal_queue_.stop_lifo_append();
  return true;
}

} // namespace caf::detail
