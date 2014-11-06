/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
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

#ifndef CAF_ABSTRACT_CHANNEL_HPP
#define CAF_ABSTRACT_CHANNEL_HPP

#include "caf/fwd.hpp"
#include "caf/node_id.hpp"
#include "caf/message_id.hpp"
#include "caf/ref_counted.hpp"

namespace caf {

/**
 * Interface for all message receivers. * This interface describes an
 * entity that can receive messages and is implemented by {@link actor}
 * and {@link group}.
 */
class abstract_channel : public ref_counted {
 public:
  virtual ~abstract_channel();

  /**
   * Enqueues a new message to the channel.
   */
  virtual void enqueue(const actor_addr& sender, message_id mid,
                       message content, execution_unit* host) = 0;

  /**
   * Returns the ID of the node this actor is running on.
   */
  inline node_id node() const {
    return m_node;
  }

  /**
   * Returns true if {@link node_ptr} returns
   */
  bool is_remote() const;

 protected:

  abstract_channel(size_t initial_ref_count = 0);

  abstract_channel(node_id nid, size_t initial_ref_count = 0);

 private:

  // identifies the node of this channel
  node_id m_node;

};

/**
 * A smart pointer to an abstract channel.
 * @relates abstract_channel_ptr
 */
using abstract_channel_ptr = intrusive_ptr<abstract_channel>;

} // namespace caf

#endif // CAF_ABSTRACT_CHANNEL_HPP
