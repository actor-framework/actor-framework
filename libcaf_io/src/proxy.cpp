/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/io/basp/proxy.hpp"

#include "caf/binary_serializer.hpp"
#include "caf/detail/sync_request_bouncer.hpp"
#include "caf/intrusive/task_result.hpp"
#include "caf/io/basp/header.hpp"
#include "caf/io/basp/message_type.hpp"
#include "caf/logger.hpp"
#include "caf/scheduler/abstract_coordinator.hpp"
#include "caf/send.hpp"

namespace caf {
namespace io {
namespace basp {

// -- constructors and destructors ---------------------------------------------

proxy::proxy(actor_config& cfg, actor dispatcher)
  : super(cfg),
    mailbox_(unit, unit, unit, unit, nullptr),
    dispatcher_(std::move(dispatcher)) {
  // Tell the dispatcher we have proxy now that needs monitoring of the remote
  // actor it represents.
  anon_send(dispatcher_, monitor_atom::value, ctrl());
  // All proxies behave as-if spawned with `lazy_init`.
  mailbox_.try_block();
}

proxy::~proxy() {
  // nop
}

// -- implementation of actor_proxy --------------------------------------------

void proxy::enqueue(mailbox_element_ptr ptr, execution_unit* eu) {
  CAF_ASSERT(ptr != nullptr);
  CAF_LOG_TRACE(CAF_ARG(*ptr));
  CAF_LOG_SEND_EVENT(ptr);
  auto mid = ptr->mid;
  auto sender = ptr->sender;
  switch (mailbox_.push_back(std::move(ptr))) {
    case intrusive::inbox_result::unblocked_reader: {
      CAF_LOG_ACCEPT_EVENT(true);
      // add a reference count to this actor and re-schedule it
      intrusive_ptr_add_ref(ctrl());
      if (eu != nullptr)
        eu->exec_later(this);
      else
        home_system().scheduler().enqueue(this);
      break;
    }
    case intrusive::inbox_result::queue_closed: {
      CAF_LOG_REJECT_EVENT();
      if (mid.is_request()) {
        detail::sync_request_bouncer f{exit_reason()};
        f(sender, mid);
      }
      break;
    }
    case intrusive::inbox_result::success:
      // enqueued to a running actors' mailbox; nothing to do
      CAF_LOG_ACCEPT_EVENT(false);
      break;
  }
}

mailbox_element* proxy::peek_at_next_mailbox_element() {
  return mailbox_.closed() || mailbox_.blocked() ? nullptr : mailbox_.peek();
}

bool proxy::add_backlink(abstract_actor* x){
  if (monitorable_actor::add_backlink(x)) {
    anon_send(this, link_atom::value, x->ctrl());
    return true;
  }
  return false;
}

bool proxy::remove_backlink(abstract_actor* x){
  if (monitorable_actor::remove_backlink(x)) {
    anon_send(this, unlink_atom::value, x->ctrl());
    return true;
  }
  return false;
}

void proxy::on_cleanup(const error& reason) {
  CAF_LOG_TRACE(CAF_ARG(reason));
  CAF_IGNORE_UNUSED(reason);
  dispatcher_ = nullptr;
}

void proxy::kill_proxy(execution_unit*, error err) {
  anon_send(this, sys_atom::value, exit_msg{nullptr, std::move(err)});
}

// -- implementation of resumable ----------------------------------------------

namespace {

class mailbox_visitor {
public:
  mailbox_visitor(proxy& self, size_t& handled_msgs, size_t max_throughput)
    : self_(self),
      handled_msgs_(handled_msgs),
      max_throughput_(max_throughput) {
    // nop
  }

  intrusive::task_result operator()(mailbox_element& x) {
    CAF_LOG_TRACE(CAF_ARG(x));
    // Check for kill_proxy event.
    if (x.content().match_elements<sys_atom, exit_msg>()) {
      auto& err = x.content().get_mutable_as<exit_msg>(1);
      self_.cleanup(std::move(err.reason), self_.context());
      return intrusive::task_result::stop_all;
    }
    // Stores the payload.
    std::vector<char> buf;
    binary_serializer sink{self_.home_system(), buf};
    // Differntiate between routed and direct messages based on the sender.
    message_type msg_type;
    if (x.sender == nullptr || x.sender->node() == self_.home_system().node()) {
      msg_type = message_type::direct_message;
      if (auto err = sink(x.stages)) {
        CAF_LOG_ERROR("cannot serialize stages:" << x);
        return intrusive::task_result::stop_all;
      }
      if (auto err = message::save(sink, x.content())) {
        CAF_LOG_ERROR("cannot serialize content:" << x);
        return intrusive::task_result::stop_all;
      }
    } else {
      msg_type = message_type::routed_message;
      if (auto err = sink(x.sender->node(), self_.node(), x.stages)) {
        CAF_LOG_ERROR("cannot serialize source, destination, and stages:" << x);
        return intrusive::task_result::stop_all;
      }
      if (auto err = message::save(sink, x.content())) {
        CAF_LOG_ERROR("cannot serialize content:" << x);
        return intrusive::task_result::stop_all;
      }
    }
    // Fill in the header and ship the message to the BASP broker.
    header hdr{msg_type,
               0,
               static_cast<uint32_t>(buf.size()),
               x.mid.integer_value(),
               x.sender == nullptr ? 0 : x.sender->id(),
               self_.id()};
    anon_send(self_.dispatcher(), self_.ctrl(), hdr, std::move(buf));
    return ++handled_msgs_ < max_throughput_ ? intrusive::task_result::resume
                                             : intrusive::task_result::stop_all;
  }

  template <class Queue>
  intrusive::task_result operator()(size_t, Queue&, mailbox_element& x) {
    return (*this)(x);
  }

  template <class OuterQueue, class InnerQueue>
  intrusive::task_result operator()(size_t, OuterQueue&, stream_slot,
                                    InnerQueue&, mailbox_element& x) {
    return (*this)(x);
  }

private:
  proxy& self_;
  size_t& handled_msgs_;
  size_t max_throughput_;
};

} // namespace

resumable::resume_result proxy::resume(execution_unit* ctx, size_t max_throughput) {
  CAF_PUSH_AID(id());
  CAF_LOG_TRACE(CAF_ARG(max_throughput));
  context_ = ctx;
  size_t handled_msgs = 0;
  mailbox_visitor f{*this, handled_msgs, max_throughput};
  while (handled_msgs < max_throughput) {
    CAF_LOG_DEBUG("start new DRR round");
    // TODO: maybe replace '3' with configurable / adaptive value?
    // Dispatch on the different message categories in our mailbox.
    if (!mailbox_.new_round(3, f).consumed_items) {
      // Check whether cleanup() was called.
      if (dispatcher_ == nullptr)
        return resumable::done;
      if (mailbox_.try_block())
        return resumable::awaiting_message;
    }
  }
  CAF_LOG_DEBUG("max throughput reached");
  return mailbox_.try_block() ? resumable::awaiting_message
                              : resumable::resume_later;
}

void proxy::intrusive_ptr_add_ref_impl() {
  intrusive_ptr_add_ref(ctrl());
}

void proxy::intrusive_ptr_release_impl() {
  intrusive_ptr_release(ctrl());
}

} // namespace basp
} // namespace io
} // namespace caf
