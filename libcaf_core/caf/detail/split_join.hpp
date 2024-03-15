// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/actor.hpp"
#include "caf/actor_system.hpp"
#include "caf/event_based_actor.hpp"

#include <vector>

namespace caf::detail {

using actor_msg_vec = std::vector<std::pair<actor, message>>;

template <class T, class Split, class Join>
class split_join_collector : public event_based_actor {
public:
  split_join_collector(actor_config& cfg, T init_value, Split s, Join j,
                       actor_msg_vec xs)
    : event_based_actor(cfg),
      workset_(std::move(xs)),
      awaited_results_(workset_.size()),
      join_(std::move(j)),
      split_(std::move(s)),
      value_(std::move(init_value)) {
    // nop
  }

  behavior make_behavior() override {
    auto f = [this](scheduled_actor*, message& msg) -> result<message> {
      auto rp = this->make_response_promise();
      split_(workset_, msg);
      for (auto& x : workset_)
        this->send(x.first, std::move(x.second));
      auto g = [this, rp](scheduled_actor*,
                          message& res) mutable -> result<message> {
        join_(value_, res);
        if (--awaited_results_ == 0) {
          rp.deliver(value_);
          quit();
        }
        return delegated<message>{};
      };
      set_default_handler(g);
      return delegated<message>{};
    };
    set_default_handler(f);
    return {[] {
      // nop
    }};
  }

private:
  actor_msg_vec workset_;
  size_t awaited_results_;
  Join join_;
  Split split_;
  T value_;
};

struct nop_split {
  void operator()(actor_msg_vec& xs, message& y) const {
    for (auto& x : xs) {
      x.second = y;
    }
  }
};

template <class T, class Split, class Join>
class split_join {
public:
  split_join(T init_value, Split s, Join j)
    : init_(std::move(init_value)), sf_(std::move(s)), jf_(std::move(j)) {
    // nop
  }

  void operator()(actor_system& sys, std::unique_lock<std::shared_mutex>& ulock,
                  const std::vector<actor>& workers, mailbox_element_ptr& ptr,
                  scheduler* sched) {
    if (!ptr->sender)
      return;
    actor_msg_vec xs;
    xs.reserve(workers.size());
    for (const auto& worker : workers)
      xs.emplace_back(worker, message{});
    ulock.unlock();
    using collector_t = split_join_collector<T, Split, Join>;
    auto hdl = sys.spawn<collector_t, lazy_init>(init_, sf_, jf_,
                                                 std::move(xs));
    hdl->enqueue(std::move(ptr), sched);
  }

private:
  T init_;
  Split sf_; // split function
  Join jf_;  // join function
};

} // namespace caf::detail
