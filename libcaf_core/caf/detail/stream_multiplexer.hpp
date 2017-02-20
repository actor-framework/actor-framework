/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
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

#ifndef CAF_DETAIL_STREAM_MULTIPLEXER_HPP
#define CAF_DETAIL_STREAM_MULTIPLEXER_HPP

#include <deque>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <unordered_map>

#include "caf/node_id.hpp"
#include "caf/optional.hpp"
#include "caf/stream_msg.hpp"
#include "caf/message_id.hpp"
#include "caf/local_actor.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/actor_control_block.hpp"

namespace caf {
namespace detail {

// Forwards messages from local actors to a remote stream_serv.
class stream_multiplexer {
public:
  /// Represents a path to a remote spawn server and stores required meta data.
  struct remote_path {
    remote_path(remote_path&&) = default;
    remote_path& operator=(remote_path&&) = default;

    inline remote_path(strong_actor_ptr ptr)
        : hdl(ptr),
          credit(1),
          in_flight(1) {
      // We start at credit 1 and in_flight 1. This means sending the first
      // message to a remote spawn server does not require previous handshaking
      // (other than establishing a connection).
    }

    /// Handle to a remote stream server.
    strong_actor_ptr hdl;
    /// Buffer for outgoing messages (sent to the BASP broker).
    std::deque<mailbox_element_ptr> buf;
    /// Available credit for sending messages.
    int32_t credit;
    /// Capacity that we have granted the remote stream server.
    int32_t in_flight;
  };

  ///  Maps node IDs to remote paths.
  using remote_paths = std::unordered_map<node_id, remote_path>;

  /// The backend of a stream server downstream establishes connections to
  /// remote stream servers via node ID.
  class backend {
  public:
    backend(actor basp_ref);

    virtual ~backend();

    /// Returns a remote actor representing the stream serv of node `nid`.
    /// Returns an invalid handle if a) `nid` is invalid or identitifies this
    /// node, or b) the backend could not establish a connection.
    virtual strong_actor_ptr remote_stream_serv(const node_id& nid) = 0;

    /// Returns a reference to the basp_ broker.
    inline actor& basp() {
      return basp_;
    }

    /// Returns all known remote stream servers and available credit.
    inline remote_paths& remotes() {
      return remotes_;
    }

    inline const remote_paths& remotes() const {
      return remotes_;
    }

    /// Queries whether `x` is a known remote node.
    bool has_remote_path(const node_id& x) const {
      return remotes().count(x) > 0;
    }

    /// Adds `ptr` as remote stream serv on `x`. This is a no-op if `x` already
    /// has a known path.
    void add_remote_path(const node_id& x, strong_actor_ptr ptr) {
      remotes().emplace(x, std::move(ptr));
    }

    /// Called whenever `nid` grants us `x` more credit.
    // @pre `current_stream_state_ != streams_.end()`
    void add_credit(const node_id& nid, int32_t x);

    // Drains as much from the buffer by sending messages to the remote
    // spawn_serv as possible, i.e., as many messages as credit is available.
    void drain_buf(remote_path& path);

  protected:
    /// A reference to the basp_ broker.
    actor basp_;

    /// Known remote stream servers and available credit.
    remote_paths remotes_;
  };

  /// Stores previous and next stage for a stream as well as the corresponding
  /// remote path.
  struct stream_state {
    strong_actor_ptr prev_stage;
    strong_actor_ptr next_stage;
    remote_path* rpath;
  };

  // Maps stream ID to stream states.
  using stream_states = std::unordered_map<stream_id, stream_state>;

  /// Creates a new stream multiplexer for `self`, using `service` to connect to
  /// remote spawn servers, and `basp` to send messages to remotes.
  /// @pre `self != nullptr && basp != nullptr`
  stream_multiplexer(local_actor* self, backend& service);

  /// Queries whether stream `x` is managed by this multiplexer.
  inline bool has_stream(const stream_id& x) const {
    return streams_.count(x) > 0;
  }

  /// Queries the number of open streams.
  inline size_t num_streams() const {
    return streams_.size();
  }

protected:
  // Dispatches `x` on the subtype `T`.
  template <class T>
  static void dispatch(T& derived, stream_msg& x) {
    // Reject anonymous messages.
    auto prev = derived.self_->current_sender();
    if (prev != nullptr) {
      // Set state for the message handlers.
      derived.current_stream_msg_ = &x;
      derived.current_stream_state_ = derived.streams_.find(x.sid);
      // Make sure that handshakes are not received twice and drop
      // non-handshake messages if no state for the stream is found.
      if (holds_alternative<stream_msg::open>(x.content)) {
        if (derived.current_stream_state_ == derived.streams_.end()) {
          derived(get<stream_msg::open>(x.content));
        } else {
          CAF_LOG_ERROR("Received multiple handshakes for stream.");
          derived.fail(sec::upstream_already_exists);
        }
      } else {
        if (derived.current_stream_state_ != derived.streams_.end()) {
          apply_visitor(derived, x.content);
        } else {
          CAF_LOG_ERROR("Unable to access required stream and/or path state.");
          derived.fail(sec::invalid_stream_state);
        }
      }
    }
  }

  // Returns a reference to the remote stream server instance for `nid`
  // if a remote stream_serv is known or connecting is successful.
  optional<remote_path&> get_remote_or_try_connect(const node_id& nid);

  // Returns a reference to the stream state for `sid`.
  optional<stream_state&> state_for(const stream_id& sid);

  /// Assings new capacity (credit) to remote stream servers.
  /// @pre `current_remote_path_ != remotes().end()`
  void manage_credit();

  // Aborts the current stream with error `reason`.
  // @pre `current_stream_msg != nullptr`
  void fail(error reason, strong_actor_ptr predecessor,
            strong_actor_ptr successor = nullptr);

  // Aborts the current stream with error `reason`, assuming `state_for` returns
  // valid predecessor and successor.
  // @pre `current_stream_msg != nullptr`
  void fail(error reason);

  // Sends message `x` to the local actor `dest`.
  void send_local(strong_actor_ptr& dest, stream_msg&& x,
                  std::vector<strong_actor_ptr> stages = {},
                  message_id mid = message_id::make());

  // Creates a new message for the BASP broker.
  inline mailbox_element_ptr
  make_basp_message(remote_path& path, message&& x,
                    std::vector<strong_actor_ptr> stages = {},
                    message_id mid = message_id::make()) {
    return make_mailbox_element(self_->ctrl(), message_id::make(), {},
                                forward_atom::value,
                                strong_actor_ptr{self_->ctrl()},
                                std::move(stages), path.hdl, mid, std::move(x));
  }

  // Sends message `x` to the remote stream server `dest`.
  inline void send_remote(remote_path& path, stream_msg&& x,
                          std::vector<strong_actor_ptr> stages = {},
                          message_id mid = message_id::make()) {
    path.buf.emplace_back(make_basp_message(path, make_message(std::move(x)),
                                            std::move(stages), mid));
    service_.drain_buf(path);
  }

  // Sends the control message `x` to the remote stream server `dest`. A control
  // message signals capaticity and therefore does not use credit on its own and
  // is sent immediately.
  inline void send_remote_ctrl(remote_path& path, message&& x) {
    basp()->enqueue(make_basp_message(path, std::move(x)), self_->context());
  }

  /// Returns a reference to the basp_ broker.
  inline actor& basp() const {
    return service_.basp();
  }

  /// Returns all known remote stream servers and available credit.
  inline remote_paths& remotes() {
    return service_.remotes();
  }

  /// Returns all known remote stream servers and available credit.
  inline const remote_paths& remotes() const {
    return service_.remotes();
  }

  // Stores which stream is currently processed.
  stream_msg* current_stream_msg_ = nullptr;

  /// Stores which stream state belongs to `current_stream_msg_`.
  stream_states::iterator current_stream_state_;

  // The parent actor.
  local_actor* self_;

  // The remoting backend.
  backend& service_;

  // Open streams.
  stream_states streams_;
};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_STREAM_MULTIPLEXER_HPP
