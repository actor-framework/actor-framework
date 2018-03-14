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

#ifndef CAF_DETAIL_STREAM_DISTRIBUTION_TREE_HPP
#define CAF_DETAIL_STREAM_DISTRIBUTION_TREE_HPP

#include <memory>
#include <unordered_map>

#include "caf/broadcast_downstream_manager.hpp"
#include "caf/ref_counted.hpp"
#include "caf/scheduled_actor.hpp"
#include "caf/stream_manager.hpp"
#include "caf/stream_stage.hpp"

namespace caf {
namespace detail {

/// A stream distribution tree consist of peers forming an acyclic graph. The
/// user is responsible for making sure peers do not form a loop. Data is
/// flooded along the tree. Each peer serves any number of subscribers. The
/// policy of the tree enables subscriptions to different chunks of the whole
/// stream (substreams).
///
/// The tree uses two CAF streams between each pair of peers for transmitting
/// data. This automatically adds backpressure to the system, i.e., no peer can
/// overwhelm others.
///
/// Policies need to provide the following member types and functions:
///
/// ~~~{.cpp}
/// TODO
/// };
/// ~~~
template <class Policy>
class stream_distribution_tree : public stream_manager {
public:
  // -- nested types -----------------------------------------------------------

  using super = stream_manager;

  using downstream_manager_type = typename Policy::downstream_manager_type;

  // --- constructors and destructors ------------------------------------------

  template <class... Ts>
  stream_distribution_tree(scheduled_actor* selfptr, Ts&&... xs)
      : super(selfptr),
        out_(selfptr),
        policy_(this, std::forward<Ts>(xs)...) {
    continuous(true);
  }

  ~stream_distribution_tree() override {
    // nop
  }

  // -- Accessors --------------------------------------------------------------

  inline Policy& policy() {
    return policy_;
  }

  inline const Policy& policy() const {
    return policy_;
  }

  void close_remote_input();


  // -- Overridden member functions of `stream_manager` ------------------------

  /// Terminates the core actor with log output `log_message` if `at_end`
  /// returns `true`.
  /*
  void shutdown_if_at_end(const char* log_message) {
    CAF_IGNORE_UNUSED(log_message);
    if (policy_.at_end()) {
      CAF_LOG_DEBUG(log_message);
      self_->quit(caf::exit_reason::user_shutdown);
    }
  }
  */

  // -- overridden member functions of `stream_manager` ------------------------

  void handle(inbound_path* path, downstream_msg::batch& x) override {
    CAF_LOG_TRACE(CAF_ARG(x));
    auto slot = path->slots.receiver;
    policy_.before_handle_batch(slot, path->hdl);
    policy_.handle_batch(slot, path->hdl, x.xs);
    policy_.after_handle_batch(slot, path->hdl);
  }

  void handle(inbound_path* from, downstream_msg::close&) override {
    policy_.path_closed(from->slots.receiver);
  }

  void handle(inbound_path* from, downstream_msg::forced_close& x) override {
    policy_.path_force_closed(from->slots.receiver, x.reason);
  }

  bool handle(stream_slots slots, upstream_msg::ack_open& x) override {
    CAF_LOG_TRACE(CAF_ARG(slots) << CAF_ARG(x));
    auto rebind_from = x.rebind_from;
    auto rebind_to = x.rebind_to;
    if (super::handle(slots, x)) {
      policy_.ack_open_success(slots.receiver, rebind_from, rebind_to);
      return true;
    }
    policy_.ack_open_failure(slots.receiver, rebind_from, rebind_to);
    return false;
  }

  void handle(stream_slots slots, upstream_msg::drop&) override {
    auto slot = slots.receiver;
    if (out().remove_path(slots.receiver, none, true))
      policy_.path_dropped(slot);
  }

  void handle(stream_slots slots, upstream_msg::forced_drop& x) override {
    auto slot = slots.receiver;
    if (out().remove_path(slots.receiver, x.reason, true))
      policy_.path_force_dropped(slot, x.reason);
  }

  bool done() const override {
    return !continuous() && pending_handshakes_ == 0
           && inbound_paths_.empty() && out_.clean();
  }

  downstream_manager_type& out() override {
    return out_;
  }

private:
  downstream_manager_type out_;
  Policy policy_;
};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_STREAM_DISTRIBUTION_TREE_HPP
