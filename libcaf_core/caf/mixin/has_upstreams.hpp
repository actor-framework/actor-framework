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

#ifndef CAF_MIXIN_HAS_UPSTREAMS_HPP
#define CAF_MIXIN_HAS_UPSTREAMS_HPP

#include "caf/sec.hpp"
#include "caf/expected.hpp"
#include "caf/stream_id.hpp"
#include "caf/stream_priority.hpp"
#include "caf/actor_control_block.hpp"

namespace caf {
namespace mixin {

/// Mixin for streams with has number of upstream actors.
template <class Base, class Subtype>
class has_upstreams : public Base {
public:
  expected<size_t> add_upstream(strong_actor_ptr& ptr, const stream_id& sid,
                                stream_priority prio) final {
    if (!ptr)
      return sec::invalid_argument;
    return in().add_path(ptr, sid, prio);
  }

  error close_upstream(strong_actor_ptr& ptr) final {
    if (in().remove_path(ptr)) {
      if (in().closed())
        dptr()->last_upstream_closed();
      return none;
    }
    return sec::invalid_upstream;
  }

  error pull(size_t amount) final {
    return in().pull(amount);
  }

private:
  Subtype* dptr() {
    return static_cast<Subtype*>(this);
  }

  abstract_upstream& in() {
    return dptr()->in();
  }
};

} // namespace mixin
} // namespace caf

#endif // CAF_MIXIN_HAS_UPSTREAMS_HPP
