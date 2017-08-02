/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
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

#ifndef CAF_DETAIL_STREAM_DISTRIBUTION_TREE_HPP
#define CAF_DETAIL_STREAM_DISTRIBUTION_TREE_HPP

#include <memory>
#include <unordered_map>

#include "caf/ref_counted.hpp"
#include "caf/stream_manager.hpp"
#include "caf/random_gatherer.hpp"
#include "caf/scheduled_actor.hpp"
#include "caf/broadcast_scatterer.hpp"

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
/// struct policy {
///   /// Any number of user-defined arguments.
///   policy(...);
///
///   /// Tuple of available substream scatterers to subscribers. Each
///   /// element of the tuple is a subtype of `topic_scatterer<T>` (or
///   /// provides a similar interface).
///   using substream_scatterers = ...;
///
///   /// A compile-time index type identifying individual substreams.
///   using substream_index_type = ...;
///
///   /// Represent a single topic for filtering stream data.
///   using topic_type = ...;
///
///   /// Groups multiple topics into a single selection filter.
///   using filter_type = ...;
///
///   /// Policy for gathering data from peers.
///   using gatherer_type = ...;
///
///   /// Policy for scattering data to peers.
///   using scatterer_type = ...;
///
///   /// Decides whether a filter applies to a given message.
///   bool selected(const filter_type& sieve, const message& sand) const;
///
///   // + one overload to `selected` for each substream data type.
///
///   /// Accesses individual substream scatterers by index.
///   auto& substream(substream_scatterers&, substream_index_type);
///
///   /// Accesses individual substream scatterers by stream ID.
///   stream_scatterer* substream(substream_scatterers&, const stream_id&);
///
///   /// Returns true if the substreams have no data pending and the policy
///   /// decides to shutdown this peer.
///   bool at_end() const;
///
///   /// Returns true if outgoing data originating on this peer is
///   /// forwarded to all substreams. Otherwise, the substreams receive data
///   /// produced on other peers exclusively.
///   bool substream_local_data() const;
///
///   /// Handles a batch if matches the data types managed by any of the
///   /// substreams. Returns `none` if the batch is a peer message, otherwise
///   /// the error resulting from handling the batch.
///   optional<error> batch(const stream_id& sid, const actor_addr& hdl,
///                         long xs_size, message& xs, int64_t xs_id);
///
///   ///
///   void push_to_substreams(message msg);
/// };
/// ~~~
template <class Policy>
class stream_distribution_tree : public stream_manager {
public:
  // -- nested types -----------------------------------------------------------
  
  using super = stream_manager;

  using gatherer_type = typename Policy::gatherer_type;

  using scatterer_type = typename Policy::scatterer_type;

  // --- constructors and destructors ------------------------------------------

  template <class... Ts>
  stream_distribution_tree(scheduled_actor* selfptr, Ts&&... xs)
      : self_(selfptr),
        in_(selfptr),
        out_(selfptr),
        policy_(this, std::forward<Ts>(xs)...) {
    // nop
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
  void shutdown_if_at_end(const char* log_message) {
    CAF_IGNORE_UNUSED(log_message);
    if (policy_.at_end()) {
      CAF_LOG_DEBUG(log_message);
      self_->quit(caf::exit_reason::user_shutdown);
    }
  }

  // -- overridden member functions of `stream_manager` ------------------------

  error ack_open(const stream_id& sid, const actor_addr& rebind_from,
                 strong_actor_ptr rebind_to, long initial_demand,
                 bool redeployable) override {
    CAF_LOG_TRACE(CAF_ARG(sid) << CAF_ARG(rebind_from) << CAF_ARG(rebind_to)
                  << CAF_ARG(initial_demand) << CAF_ARG(redeployable));
    auto res = super::ack_open(sid, rebind_from, rebind_to, initial_demand,
                               redeployable);
    if (res == none)
      policy_.ack_open_success(sid, rebind_from, rebind_to);
    else
      policy_.ack_open_failure(sid, rebind_from, rebind_to, res);
    return res;
  }

  error process_batch(message& xs) override {
    CAF_LOG_TRACE(CAF_ARG(xs));
    policy_.handle_batch(xs);
    return none;
  }

  error batch(const stream_id& sid, const actor_addr& hdl, long xs_size,
              message& xs, int64_t xs_id) override {
    CAF_LOG_TRACE(CAF_ARG(sid) << CAF_ARG(hdl) << CAF_ARG(xs_size)
                  << CAF_ARG(xs) << CAF_ARG(xs_id));
    policy_.before_handle_batch(sid, hdl, xs_size, xs, xs_id);
    auto res = super::batch(sid, hdl, xs_size, xs, xs_id);
    policy_.after_handle_batch(sid, hdl, xs_id);
    push();
    return res;
  }

  bool done() const override {
    return false;
  }

  gatherer_type& in() override {
    return in_;
  }

  scatterer_type& out() override {
    return out_;
  }

  void downstream_demand(outbound_path*, long) override {
    CAF_LOG_TRACE("");
    push();
    in_.assign_credit(out_.credit());
  }

  error close(const stream_id& sid, const actor_addr& hdl) override {
    CAF_LOG_TRACE(CAF_ARG(sid) << CAF_ARG(hdl));
    if (in_.remove_path(sid, hdl, none, true))
      return policy_.path_closed(sid, hdl);
    return none;
  }

  error drop(const stream_id& sid, const actor_addr& hdl) override {
    CAF_LOG_TRACE(CAF_ARG(sid) << CAF_ARG(hdl));
    if (out_.remove_path(sid, hdl, none, true))
      return policy_.path_dropped(sid, hdl);
    return none;
  }

  error forced_close(const stream_id& sid, const actor_addr& hdl,
                     error reason) override {
    CAF_LOG_TRACE(CAF_ARG(sid) << CAF_ARG(hdl) << CAF_ARG(reason));
    if (in_.remove_path(sid, hdl, reason, true))
      return policy_.path_force_closed(sid, hdl, std::move(reason));
    return none;
  }

  error forced_drop(const stream_id& sid, const actor_addr& hdl,
                     error reason) override {
    if (out_.remove_path(sid, hdl, reason, true))
      return policy_.path_force_dropped(sid, hdl, std::move(reason));
    return none;
  }

  scheduled_actor* self() {
    return self_;
  }

private:
  scheduled_actor* self_;
  gatherer_type in_;
  scatterer_type out_;
  Policy policy_;
};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_STREAM_DISTRIBUTION_TREE_HPP
