/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_DETAIL_SPLIT_JOIN_HPP
#define CAF_DETAIL_SPLIT_JOIN_HPP

#include <vector>

#include "caf/actor.hpp"
#include "caf/locks.hpp"
#include "caf/spawn.hpp"
#include "caf/event_based_actor.hpp"

#include "caf/detail/shared_spinlock.hpp"

namespace caf {
namespace detail {

using actor_msg_vec = std::vector<std::pair<actor, message>>;

template <class T, class Split, class Join>
class split_join_collector : public event_based_actor {
 public:
  split_join_collector(T init_value, Split s, Join j, actor_msg_vec xs)
      : m_workset(std::move(xs)),
        m_awaited_results(m_workset.size()),
        m_join(std::move(j)),
        m_split(std::move(s)),
        m_value(std::move(init_value)) {
    // nop
  }

  behavior make_behavior() override {
    return {
      others >> [=] {
        m_rp = this->make_response_promise();
        m_split(m_workset, this->current_message());
        // first message is the forwarded request
        // GCC 4.7 workaround; `for (auto& x : m_workset)` doesn't compile
        for (size_t i = 0; i < m_workset.size(); ++i) {
          auto& x = m_workset[i];
          this->sync_send(x.first, std::move(x.second)).then(
            others >> [=] {
              m_join(m_value, this->current_message());
              if (--m_awaited_results == 0) {
                m_rp.deliver(make_message(m_value));
                quit();
              }
            }
          );
        }
        // no longer needed
        m_workset.clear();
      }
    };
  }

 private:
  actor_msg_vec m_workset;
  size_t m_awaited_results;
  Join m_join;
  Split m_split;
  T m_value;
  response_promise m_rp;
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
      : m_init(std::move(init_value)),
        m_sf(std::move(s)),
        m_jf(std::move(j)) {
    // nop
  }

  void operator()(upgrade_lock<detail::shared_spinlock>& ulock,
                  const std::vector<actor>& workers,
                  mailbox_element_ptr& ptr,
                  execution_unit* host) {
    if (ptr->sender == invalid_actor_addr) {
      return;
    }
    actor_msg_vec xs(workers.size());
    for (size_t i = 0; i < workers.size(); ++i) {
      xs[i].first = workers[i];
    }
    ulock.unlock();
    using collector_t = split_join_collector<T, Split, Join>;
    auto hdl = spawn<collector_t, lazy_init>(m_init, m_sf, m_jf, std::move(xs));
    hdl->enqueue(std::move(ptr), host);
  }

 private:
  T m_init;
  Split m_sf; // split function
  Join m_jf;  // join function
};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_SPLIT_JOIN_HPP

