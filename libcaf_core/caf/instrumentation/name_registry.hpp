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

#ifndef CAF_SIGNATURE_REGISTRY_HPP
#define CAF_SIGNATURE_REGISTRY_HPP

#include "caf/instrumentation/instrumentation_ids.hpp"
#include "caf/type_erased_tuple.hpp"
#include "caf/message.hpp"

#include <string>
#include <cstdint>
#include <typeinfo>
#include <unordered_map>

namespace caf {
namespace instrumentation {

class name_registry {
public:
  actortype_id get_actortype(const std::type_info& ti);
  std::string identify_actortype(actortype_id cs) const;
  callsite_id get_simple_signature(const type_erased_tuple& m);
  callsite_id get_simple_signature(const message& m);
  std::string identify_simple_signature(callsite_id cs) const;

private:
  std::unordered_map<actortype_id, std::string> actortypes_;
  std::unordered_map<callsite_id, std::string> signatures_;
};

} // namespace instrumentation
} // namespace caf

#endif // CAF_SIGNATURE_REGISTRY_HPP
