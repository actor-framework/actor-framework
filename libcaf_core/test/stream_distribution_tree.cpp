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

#include <vector>
#include <string>
#include <numeric>
#include <utility>
#include <fstream>
#include <iostream>
#include <iterator>

#define CAF_SUITE stream_distribution_tree
#include "caf/test/dsl.hpp"

#include "caf/fused_scatterer.hpp"
#include "caf/broadcast_topic_scatterer.hpp"

#include "caf/detail/pull5_gatherer.hpp"
#include "caf/detail/push5_scatterer.hpp"
#include "caf/detail/stream_distribution_tree.hpp"

using std::cout;
using std::endl;
using std::vector;
using std::string;

using namespace caf::detail;
using namespace caf;

namespace {

using join_atom = atom_constant<atom("join")>;
using peer_atom = atom_constant<atom("peer")>;

struct prefix_match_t {
  using topic_type = string;
  using filter_type = vector<string>;

  bool operator()(const filter_type& filter,
                  const topic_type& topic) const {
    for (auto& prefix : filter)
      if (topic.compare(0, prefix.size(), prefix.c_str()) == 0)
        return true;
    return false;
  }

  template <class T>
  bool operator()(const filter_type& filter,
                  const std::pair<topic_type, T>& x) const {
    return (*this)(filter, x.first);
  }

  bool operator()(const filter_type& filter, const message& msg) const {
    if (!msg.match_element<topic_type>(0))
      return false;
    return (*this)(filter, msg.get_as<topic_type>(0));
  }
};

constexpr prefix_match_t prefix_match = prefix_match_t{};

using peer_filter_type = std::pair<actor_addr, vector<string>>;

struct peer_filter_cmp {
  actor_addr active_sender;
  template <class T>
  bool operator()(const peer_filter_type& f, const T& x) const {
    return f.first != active_sender && prefix_match(f.second, x);
  }
};

class policy {
public:
  using data = int;
  using internal_command = string;
  using topic = string;
  using filter_type = vector<string>;
  using gatherer_type = detail::pull5_gatherer;

  using peer_batch = std::vector<message>;
  using worker_batch = std::vector<std::pair<topic, data>>;
  using store_batch = std::vector<std::pair<topic, internal_command>>;

  using path_id = std::pair<stream_id, actor_addr>;

  using peer_to_path_map = std::map<actor, path_id>;

  using path_to_peer_map = std::map<path_id, actor>;

  template <class T>
  using substream_t = broadcast_topic_scatterer<std::pair<topic, T>,
                                                filter_type, prefix_match_t>;

  using main_stream_t = broadcast_topic_scatterer<message, peer_filter_type,
                                                  peer_filter_cmp>;

  using scatterer_type = fused_scatterer<main_stream_t, substream_t<data>,
                                         substream_t<internal_command>>;

  policy(stream_distribution_tree<policy>* parent, filter_type filter);

  /// Returns true if 1) `shutting_down()`, 2) there is no more
  /// active local data source, and 3) there is no pending data to any peer.
  bool at_end() const {
    return shutting_down_ && peers().paths_clean()
           && workers().paths_clean() && stores().paths_clean();
  }

  bool substream_local_data() const {
    return false;
  }

  void before_handle_batch(const stream_id&, const actor_addr& hdl,
                           long, message&, int64_t) {
    parent_->out().main_stream().selector().active_sender = hdl;
  }

  void handle_batch(message& xs) {
    if (xs.match_elements<peer_batch>()) {
      // Only received from other peers. Extract content for to local workers
      // or stores and then forward to other peers.
      for (auto& msg : xs.get_mutable_as<peer_batch>(0)) {
        // Extract worker messages.
        if (msg.match_elements<topic, data>())
          workers().push(msg.get_as<topic>(0), msg.get_as<data>(1));
        // Extract store messages.
        if (msg.match_elements<topic, internal_command>())
          stores().push(msg.get_as<topic>(0), msg.get_as<internal_command>(1));
        // Forward to other peers.
        peers().push(std::move(msg));
      }
    } else if (xs.match_elements<worker_batch>()) {
      // Inputs from local workers are only forwarded to peers.
      for (auto& x : xs.get_mutable_as<worker_batch>(0)) {
        parent_->out().push(make_message(std::move(x.first),
                                         std::move(x.second)));
      }
    } else if (xs.match_elements<store_batch>()) {
      // Inputs from stores are only forwarded to peers.
      for (auto& x : xs.get_mutable_as<store_batch>(0)) {
        parent_->out().push(make_message(std::move(x.first),
                                         std::move(x.second)));
      }
    } else {
      CAF_LOG_ERROR("unexpected batch:" << deep_to_string(xs));
    }
  }

  void after_handle_batch(const stream_id&, const actor_addr&, int64_t) {
    parent_->out().main_stream().selector().active_sender = nullptr;
  }

  void ack_open_success(const stream_id& sid, const actor_addr& rebind_from,
                        strong_actor_ptr rebind_to) {
    auto old_id = std::make_pair(sid, rebind_from);
    auto new_id = std::make_pair(sid, actor_cast<actor_addr>(rebind_to));
    auto i = opath_to_peer_.find(old_id);
    if (i != opath_to_peer_.end()) {
      auto pp = std::move(i->second);
      peer_to_opath_[pp] = new_id;
      opath_to_peer_.erase(i);
      opath_to_peer_.emplace(new_id, std::move(pp));
    }
  }

  void ack_open_failure(const stream_id& sid, const actor_addr& rebind_from,
                        strong_actor_ptr rebind_to, const error&) {
    auto old_id = std::make_pair(sid, rebind_from);
    auto new_id = std::make_pair(sid, actor_cast<actor_addr>(rebind_to));
    auto i = opath_to_peer_.find(old_id);
    if (i != opath_to_peer_.end()) {
      auto pp = std::move(i->second);
      peer_lost(pp);
      peer_to_opath_.erase(pp);
      opath_to_peer_.erase(i);
    }
  }

  void push_to_substreams(vector<message> vec) {
    CAF_LOG_TRACE(CAF_ARG(vec));
    // Move elements from `xs` to the buffer for local subscribers.
    if (!workers().lanes().empty())
      for (auto& x : vec)
        if (x.match_elements<topic, data>()) {
          x.force_unshare();
          workers().push(x.get_as<topic>(0),
                        std::move(x.get_mutable_as<data>(1)));
        }
    workers().emit_batches();
    if (!stores().lanes().empty())
      for (auto& x : vec)
        if (x.match_elements<topic, internal_command>()) {
          x.force_unshare();
          stores().push(x.get_as<topic>(0),
                       std::move(x.get_mutable_as<internal_command>(1)));
        }
    stores().emit_batches();
  }

  optional<error> batch(const stream_id&, const actor_addr&,
                        long, message& xs, int64_t) {
    if (xs.match_elements<std::vector<std::pair<topic, data>>>()) {
      return error{none};
    }
    if (xs.match_elements<std::vector<std::pair<topic, internal_command>>>()) {
      return error{none};
    }
    return none;
  }

  // -- callbacks --------------------------------------------------------------

  void peer_lost(const actor&) {
    // nop
  }

  void local_input_closed(const stream_id&, const actor_addr&) {
    // nop
  }

  // -- state required by the distribution tree --------------------------------

  bool shutting_down() {
    return shutting_down_;
  }

  void shutting_down(bool value) {
    shutting_down_ = value;
  }

  // -- peer management --------------------------------------------------------

  /// Adds a new peer that isn't fully initialized yet. A peer is fully
  /// initialized if there is an upstream ID associated to it.
  bool add_peer(const caf::stream_id& sid,
                const strong_actor_ptr& downstream_handle,
                const actor& peer_handle, filter_type filter) {
    CAF_LOG_TRACE(CAF_ARG(sid) << CAF_ARG(downstream_handle)
                  << CAF_ARG(peer_handle) << CAF_ARG(filter));
    //TODO: auto ptr = peers().add_path(sid, downstream_handle);
    outbound_path* ptr = nullptr;
    if (ptr == nullptr)
      return false;
    auto downstream_addr = actor_cast<actor_addr>(downstream_handle);
    peers().set_filter(sid, downstream_handle,
                       {downstream_addr, std::move(filter)});
    peer_to_opath_.emplace(peer_handle, std::make_pair(sid, downstream_addr));
    opath_to_peer_.emplace(std::make_pair(sid, downstream_addr), peer_handle);
    return true;
  }

  /// Fully initializes a peer by setting an upstream ID and inserting it into
  /// the `input_to_peer_`  map.
  bool init_peer(const stream_id& sid, const strong_actor_ptr& upstream_handle,
                 const actor& peer_handle) {
    auto upstream_addr = actor_cast<actor_addr>(upstream_handle);
    peer_to_ipath_.emplace(peer_handle, std::make_pair(sid, upstream_addr));
    ipath_to_peer_.emplace(std::make_pair(sid, upstream_addr), peer_handle);
    return true;
  }

  /// Removes a peer, aborting any stream to & from that peer.
  bool remove_peer(const actor& hdl, error reason, bool silent) {
    CAF_LOG_TRACE(CAF_ARG(hdl));
    { // lifetime scope of first iterator pair
      auto e = peer_to_opath_.end();
      auto i = peer_to_opath_.find(hdl);
      if (i == e)
        return false;
      peers().remove_path(i->second.first, i->second.second, reason, silent);
      opath_to_peer_.erase(std::make_pair(i->second.first, i->second.second));
      peer_to_opath_.erase(i);
    }
    { // lifetime scope of second iterator pair
      auto e = peer_to_ipath_.end();
      auto i = peer_to_ipath_.find(hdl);
      CAF_IGNORE_UNUSED(e);
      CAF_ASSERT(i != e);
      parent_->in().remove_path(i->second.first, i->second.second,
                                reason, silent);
      ipath_to_peer_.erase(std::make_pair(i->second.first, i->second.second));
      peer_to_ipath_.erase(i);
    }
    peer_lost(hdl);
    if (shutting_down() && peer_to_opath_.empty()) {
      // Shutdown when the last peer stops listening.
      parent_->self()->quit(caf::exit_reason::user_shutdown);
    } else {
      // See whether we can make progress without that peer in the mix.
      parent_->in().assign_credit(parent_->out().credit());
      parent_->push();
    }
    return true;
  }

  /// Updates the filter of an existing peer.
  bool update_peer(const actor& hdl, filter_type filter) {
    CAF_LOG_TRACE(CAF_ARG(hdl) << CAF_ARG(filter));
    auto e = peer_to_opath_.end();
    auto i = peer_to_opath_.find(hdl);
    if (i == e) {
      CAF_LOG_DEBUG("cannot update filter on unknown peer");
      return false;
    }
    peers().set_filter(i->second.first, i->second.second,
                       {i->second.second, std::move(filter)});
    return true;
  }

  // -- selectively pushing data into the streams ------------------------------

  /// Pushes data to workers without forwarding it to peers.
  void local_push(topic x, data y) {
    workers().push(std::move(x), std::move(y));
    workers().emit_batches();
  }

  /// Pushes data to stores without forwarding it to peers.
  void local_push(topic x, internal_command y) {
    stores().push(std::move(x), std::move(y));
    stores().emit_batches();
  }

  /// Pushes data to peers only without forwarding it to local substreams.
  void remote_push(message msg) {
    peers().push(std::move(msg));
    peers().emit_batches();
  }

  /// Pushes data to peers and workers.
  void push(topic x, data y) {
    remote_push(make_message(x, y));
    local_push(std::move(x), std::move(y));
  }

  /// Pushes data to peers and stores.
  void push(topic x, internal_command y) {
    remote_push(make_message(x, y));
    local_push(std::move(x), std::move(y));
  }

  // -- state accessors --------------------------------------------------------

  main_stream_t& peers() {
    return parent_->out().main_stream();
  }

  const main_stream_t& peers() const {
    return parent_->out().main_stream();
  }

  substream_t<data>& workers() {
    return parent_->out().substream<1>();
  }

  const substream_t<data>& workers() const {
    return parent_->out().substream<1>();
  }

  substream_t<internal_command>& stores() {
    return parent_->out().substream<2>();
  }

  const substream_t<internal_command>& stores() const {
    return parent_->out().substream<2>();
  }

private:
  stream_distribution_tree<policy>* parent_;
  bool shutting_down_;
  // Maps peer handles to output path IDs.
  peer_to_path_map peer_to_opath_;
  // Maps output path IDs to peer handles.
  path_to_peer_map opath_to_peer_;
  // Maps peer handles to input path IDs.
  peer_to_path_map peer_to_ipath_;
  // Maps input path IDs to peer handles.
  path_to_peer_map ipath_to_peer_;
};

policy::policy(stream_distribution_tree<policy>* parent, filter_type)
    : parent_(parent),
      shutting_down_(false) {
  // nop
}


policy::filter_type default_filter() {
  return {"foo", "bar"};
}

/*
behavior testee(event_based_actor* self) {
  auto sdt = make_counted<stream_distribution_tree<policy>>(self,
                                                            default_filter());
  return {

  };
}
*/

using fixture = test_coordinator_fixture<>;

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(stream_distribution_tree_tests, fixture)

CAF_TEST(peer_management) {
}

CAF_TEST_FIXTURE_SCOPE_END()
