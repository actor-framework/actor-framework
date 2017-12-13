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

#include <string>
#include <cstdint>
#include <unordered_map>

#include "caf/type_erased_tuple.hpp"

namespace caf {
namespace instrumentation {

class signature_registry {
public:
  uint64_t identify(type_erased_tuple& m);
  std::string get_human_readable_callsite(uint64_t cs) const;

private:
  std::unordered_map<uint64_t, std::string> signatures_;
};

} // namespace instrumentation
} // namespace caf

#endif // CAF_SIGNATURE_REGISTRY_HPP
