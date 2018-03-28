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

#pragma once

#include <vector>

#include "caf/actor.hpp"
#include "caf/locks.hpp"
#include "caf/actor_system.hpp"
#include "caf/event_based_actor.hpp"

#include "caf/detail/shared_spinlock.hpp"

namespace caf {
namespace detail {

using actor_msg_vec = std::vector<std::pair<actor, message>>;

template <class T, class Split, class Join>
class split_join_collector : public event_based_actor {
public:
  split_join_collector(actor_config& cfg, T init_value,
                       Split s, Join j, actor_msg_vec xs)
      : event_based_actor(cfg),
        workset_(std::move(xs)),
        awaited_results_(workset_.size()),
        join_(std::move(j)),
        split_(std::move(s)),
        value_(std::move(init_value)) {
    // nop
  }

  behavior make_behavior() override {
    auto f = [=](scheduled_actor*, message_view& xs) -> result<message> {
      auto msg = xs.move_content_to_message();
      auto rp = this->make_response_promise();
      split_(workset_, msg);
      for (auto& x : workset_)
        this->send(x.first, std::move(x.second));
      auto g = [=](scheduled_actor*, message_view& ys) mutable
               -> result<message> {
        auto res = ys.move_content_to_message();
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
    return {
      [] {
        // nop
      }
    };
  }

private:
  actor_msg_vec workset_;
  size_t awaited_results_;
  Join join_;
  Split split_;
  T value_;
};

struct nop_split {
  inline void operator()(actor_msg_vec& xs, message& y) const {
    for (auto& x : xs) {
      x.second = y;
    }
  }
};

template <class T, class Split, class Join>
class split_join {
public:
  split_join(T init_value, Split s, Join j)
      : init_(std::move(init_value)),
        sf_(std::move(s)),
        jf_(std::move(j)) {
    // nop
  }

  void operator()(actor_system& sys,
                  upgrade_lock<detail::shared_spinlock>& ulock,
                  const std::vector<actor>& workers,
                  mailbox_element_ptr& ptr,
                  execution_unit* host) {
    if (!ptr->sender)
      return;
    actor_msg_vec xs;
    xs.reserve(workers.size());
    for (const auto & worker : workers)
      xs.emplace_back(worker, message{});
    ulock.unlock();
    using collector_t = split_join_collector<T, Split, Join>;
    auto hdl = sys.spawn<collector_t, lazy_init>(init_, sf_, jf_, std::move(xs));
    hdl->enqueue(std::move(ptr), host);
  }

private:
  T init_;
  Split sf_; // split function
  Join jf_;  // join function
};

} // namespace detail
} // namespace caf


