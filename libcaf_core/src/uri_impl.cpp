/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/detail/uri_impl.hpp"

#include "caf/detail/parser/read_uri.hpp"
#include "caf/error.hpp"
#include "caf/ip_address.hpp"
#include "caf/string_algorithms.hpp"

namespace caf {
namespace detail {

// -- constructors, destructors, and assignment operators ----------------------

uri_impl::uri_impl() : rc_(1) {
  // nop
}

// -- member variables ---------------------------------------------------------

uri_impl uri_impl::default_instance;

// -- modifiers ----------------------------------------------------------------

void uri_impl::assemble_str() {
  add_encoded(scheme);
  str += ':';
  if (authority.empty()) {
    CAF_ASSERT(!path.empty());
    add_encoded(path, true);
  } else {
    str += "//";
    if (!authority.userinfo.empty()) {
      add_encoded(authority.userinfo);
      str += '@';
    }
    auto addr = get_if<ip_address>(&authority.host);
    if (addr == nullptr) {
      add_encoded(get<std::string>(authority.host));
    } else {
      str += '[';
      str += to_string(*addr);
      str += ']';
    }
    if (authority.port != 0) {
      str += ':';
      str += std::to_string(authority.port);
    }
    if (!path.empty()) {
      str += '/';
      add_encoded(path, true);
    }
  }
  if (!query.empty()) {
    str += '?';
    auto i = query.begin();
    auto add_kvp = [&](decltype(*i) kvp) {
      add_encoded(kvp.first);
      str += '=';
      add_encoded(kvp.second);
    };
    add_kvp(*i);
    for (++i; i != query.end(); ++i) {
      str += '&';
      add_kvp(*i);
    }
  }
  if (!fragment.empty()) {
    str += '#';
    add_encoded(fragment);
  }
}

void uri_impl::add_encoded(string_view x, bool is_path) {
  for (auto ch : x)
    switch (ch) {
      case '/':
        if (is_path) {
          str += ch;
          break;
        }
        // fall through
      case ' ':
      case ':':
      case '?':
      case '#':
      case '[':
      case ']':
      case '@':
      case '!':
      case '$':
      case '&':
      case '\'':
      case '"':
      case '(':
      case ')':
      case '*':
      case '+':
      case ',':
      case ';':
      case '=':
        str += '%';
        detail::append_hex(str, reinterpret_cast<uint8_t*>(&ch), 1);
        break;
      default:
        str += ch;
    }
}

// -- friend functions ---------------------------------------------------------

void intrusive_ptr_add_ref(const uri_impl* p) {
  p->rc_.fetch_add(1, std::memory_order_relaxed);
}

void intrusive_ptr_release(const uri_impl* p) {
  if (p->rc_ == 1 || p->rc_.fetch_sub(1, std::memory_order_acq_rel) == 1)
    delete p;
}

} // namespace detail
} // namespace caf
