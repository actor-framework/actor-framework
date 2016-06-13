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

#ifndef CAF_MIXIN_HAS_DOWNSTREAMS_HPP
#define CAF_MIXIN_HAS_DOWNSTREAMS_HPP

#include <cstddef>

#include "caf/sec.hpp"
#include "caf/actor_control_block.hpp"

namespace caf {
namespace mixin {

/// Mixin for streams with any number of downstreams.
template <class Base, class Subtype>
class has_downstreams : public Base {
public:
  using topics = typename Base::topics;

  error add_downstream(strong_actor_ptr& ptr, const topics& filter,
                       size_t init, bool is_redeployable) final {
    if (!ptr)
      return sec::invalid_argument;
    if (out().add_path(ptr, filter, is_redeployable)) {
      dptr()->downstream_demand(ptr, init);
      return none;
    }
    return sec::downstream_already_exists;
  }

  error trigger_send(strong_actor_ptr&) {
  if (out().buf_size() > 0)
    out().policy().push(out());
  return none;

  }

private:
  Subtype* dptr() {
    return static_cast<Subtype*>(this);
  }

  abstract_downstream& out() {
    return dptr()->out();
  }
};

} // namespace mixin
} // namespace caf

#endif // CAF_MIXIN_HAS_DOWNSTREAMS_HPP
