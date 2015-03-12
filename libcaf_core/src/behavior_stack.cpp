/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
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

#include <iterator>

#include "caf/none.hpp"
#include "caf/local_actor.hpp"
#include "caf/detail/behavior_stack.hpp"

using namespace std;

namespace caf {
namespace detail {

void behavior_stack::pop_back() {
  if (m_elements.empty()) {
    return;
  }
  m_erased_elements.push_back(std::move(m_elements.back()));
  m_elements.pop_back();
}

void behavior_stack::clear() {
  if (!m_elements.empty()) {
    std::move(m_elements.begin(), m_elements.end(),
              std::back_inserter(m_erased_elements));
    m_elements.clear();
  }
}

} // namespace detail
} // namespace caf
