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

#include <sstream>
#include <stdlib.h>

#include "caf/config.hpp"
#include "caf/exception.hpp"

#ifdef CAF_WINDOWS
#include <winerror.h>
#else
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#endif

namespace {

std::string ae_what(uint32_t reason) {
  std::ostringstream oss;
  oss << "actor exited with reason " << reason;
  return oss.str();
}

} // namespace <anonymous>

namespace caf {

caf_exception::~caf_exception() noexcept {
  // nop
}

caf_exception::caf_exception(const std::string& what_str) : m_what(what_str) {
  // nop
}

caf_exception::caf_exception(std::string&& what_str)
    : m_what(std::move(what_str)) {
  // nop
}

const char* caf_exception::what() const noexcept {
  return m_what.c_str();
}

actor_exited::~actor_exited() noexcept {
  // nop
}

actor_exited::actor_exited(uint32_t reason) : caf_exception(ae_what(reason)) {
  m_reason = reason;
}

network_error::network_error(const std::string& str) : super(str) {
  // nop
}

network_error::network_error(std::string&& str) : super(std::move(str)) {
  // nop
}

network_error::~network_error() noexcept {
  // nop
}

bind_failure::bind_failure(const std::string& str) : super(str) {
  // nop
}

bind_failure::bind_failure(std::string&& str) : super(std::move(str)) {
  // nop
}

bind_failure::~bind_failure() noexcept {
  // nop
}

} // namespace caf
