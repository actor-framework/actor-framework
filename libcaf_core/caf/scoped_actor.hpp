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

#ifndef CAF_SCOPED_ACTOR_HPP
#define CAF_SCOPED_ACTOR_HPP

#include "caf/behavior.hpp"
#include "caf/blocking_actor.hpp"

namespace caf {

/**
 * A scoped handle to a blocking actor.
 */
class scoped_actor {
 public:
  scoped_actor();

  scoped_actor(const scoped_actor&) = delete;

  explicit scoped_actor(bool hide_actor);

  ~scoped_actor();

  inline blocking_actor* operator->() const {
    return m_self.get();
  }

  inline blocking_actor& operator*() const {
    return *m_self;
  }

  inline blocking_actor* get() const {
    return m_self.get();
  }

  inline operator channel() const {
    return get();
  }

  inline operator actor() const {
    return get();
  }

  inline operator actor_addr() const {
    return get()->address();
  }

  inline actor_addr address() const {
    return get()->address();
  }

 private:
  void init(bool hide_actor);
  actor_id m_prev; // used for logging/debugging purposes only
  intrusive_ptr<blocking_actor> m_self;
};

} // namespace caf

#endif // CAF_SCOPED_ACTOR_HPP
