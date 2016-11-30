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

#include <string>
#include <utility>
#include <iostream>

#include "caf/ref_counted.hpp"
#include "caf/intrusive_ptr.hpp"

#include "caf/io/uri.hpp"

using namespace std;

namespace {

using str_bounds = std::pair<std::string::iterator, std::string::iterator>;

} // namespace <anonymous>

//    foo://example.com:8042/over/there?name=ferret#nose
//    \_/   \______________/\_________/ \_________/ \__/
//     |           |            |            |        |
//  scheme     authority       path        query   fragment
//     |   _____________________|__
//    / \ /                        \.
//    urn:example:animal:ferret:nose

namespace caf {
namespace io {

class uri_private : public ref_counted {
  friend optional<uri> uri::make(const string& uri_str);

  enum {
    default_flag,
    ipv4_flag,
    ipv6_flag
  } m_flag_;

  // complete uri
  string m_uri_;

  // uri components
  str_bounds m_path_;
  str_bounds m_query_;
  str_bounds m_scheme_;
  str_bounds m_fragment_;
  str_bounds m_authority_;

  // authority subcomponents
  str_bounds m_host_;
  str_bounds m_port_;
  str_bounds m_user_information_;

  // convenience fields
  uint16_t m_int_port_;

  void clear() {
    m_uri_.clear();
    m_path_ = make_pair(end(m_uri_), end(m_uri_));
    m_query_ = make_pair(end(m_uri_), end(m_uri_));
    m_scheme_ = make_pair(end(m_uri_), end(m_uri_));
    m_fragment_ = make_pair(end(m_uri_), end(m_uri_));
    m_authority_ = make_pair(end(m_uri_), end(m_uri_));
    m_host_ = make_pair(end(m_uri_), end(m_uri_));
    m_port_ = make_pair(end(m_uri_), end(m_uri_));
    m_user_information_ = make_pair(end(m_uri_), end(m_uri_));
  }

  // this parses the given uri to the form
  // {scheme} {authority} {path} {query} {fragment}
  bool parse_uri(const string& what) {
    auto empty = [](const str_bounds& bounds) {
      return bounds.first >= bounds.second;
    };
    m_flag_ = default_flag;
    m_uri_.clear();
    m_uri_ = what;
    m_host_ = make_pair(begin(m_uri_), begin(m_uri_));
    m_path_ = make_pair(begin(m_uri_), begin(m_uri_));
    m_port_ = make_pair(begin(m_uri_), begin(m_uri_));
    m_query_ = make_pair(begin(m_uri_), begin(m_uri_));
    m_scheme_ = make_pair(begin(m_uri_), begin(m_uri_));
    m_fragment_ = make_pair(begin(m_uri_), begin(m_uri_));
    m_authority_ = make_pair(begin(m_uri_), begin(m_uri_));
    m_user_information_ = make_pair(begin(m_uri_), begin(m_uri_));
    // find ':' for scheme boundaries
    auto from_itr = begin(m_uri_);
    auto to_itr = find(begin(m_uri_), end(m_uri_), ':');
    if (to_itr == m_uri_.end()) {
      return false;
    }
    m_scheme_ = make_pair(from_itr, to_itr);
    from_itr = to_itr + 1;
    // if the next two characters are '/', an authority follows the scheme
    char seperator[] = { '/', '/' };
    auto seperator_itr = search(from_itr, end(m_uri_),
                                begin(seperator), end(seperator));
    if (seperator_itr != end(m_uri_)) {
      from_itr += 2;
      to_itr = find_if(from_itr, end(m_uri_), [](const char& c) {
        return c == '/' || c == '#' || c == '?';
      });
      m_authority_ = make_pair(from_itr, to_itr);
      if (empty(m_authority_)) {
        return false;
      }
      // parse user info from authority
      auto at = find(m_authority_.first, m_authority_.second, '@');
      if (at != m_authority_.second) {
        m_user_information_ = make_pair(m_authority_.first, at);
        // skip the '@' character
        at += 1;
      } else {
        // reset if we found nothing
        at = m_authority_.first;
      }
      // parse host and port
      // look for closing ']' in case host is an ipv6 address
      auto col = find(at, m_authority_.second, ']');
      if (col == m_authority_.second) {
        // if no bracket is found, reset itr
        col = at;
      }
      col = find(col, m_authority_.second, ':');
      if (col != m_authority_.second) {
        m_host_ = make_pair(at, col);
        m_port_ = make_pair(col + 1, m_authority_.second);
      } else {
        m_host_ = make_pair(at, m_authority_.second);
        m_port_ = make_pair(m_authority_.second, m_authority_.second);
      }
      if (!empty(m_host_) && *m_host_.first == '[') {
        // ipv6 address
        m_flag_ = ipv6_flag;
        // erase leading "[" and trailing "]"
        m_host_.first += 1;
        m_host_.second -= 1;
      } else if (!empty(m_host_)) {
        m_flag_ = ipv4_flag;
      }
      if (empty(m_port_)) {
        m_int_port_ = 0;
      } else {
        auto port_str = string(m_port_.first, m_port_.second);
        m_int_port_ = static_cast<uint16_t>(atoi(port_str.c_str()));
      }
      from_itr = to_itr;
    }
    from_itr = find_if(from_itr, end(m_uri_), [](const char& c) {
      return c != '/';
    });
    to_itr = find_if(from_itr, end(m_uri_), [](const char& c) {
      return c == '#' || c == '?';
    });
    m_path_ = make_pair(from_itr, to_itr);
    from_itr = to_itr;
    if (from_itr != end(m_uri_) && *from_itr == '?') {
      from_itr += 1;
      to_itr = find(from_itr, end(m_uri_), '#');
      m_query_ = make_pair(from_itr, to_itr);
      from_itr = to_itr;
    }
    if (from_itr != end(m_uri_) && *from_itr == '#') {
      from_itr += 1;
      m_fragment_ = make_pair(from_itr, end(m_uri_));
    }
    return true;
  }

public:
  inline uri_private() { }

  inline const str_bounds& path() const { return m_path_; }

  inline const str_bounds& query() const { return m_query_; }

  inline const str_bounds& scheme() const { return m_scheme_; }

  inline const str_bounds& fragment() const { return m_fragment_; }

  inline const str_bounds& authority() const { return m_authority_; }

  inline const string& as_string() const { return m_uri_; }

  inline const str_bounds& host() const { return m_host_; }

  inline const str_bounds& port() const { return m_port_; }

  inline uint16_t port_as_int() const { return m_int_port_; }

  inline const str_bounds& user_information() const {
    return m_user_information_;
  }

  inline bool host_is_ipv4addr() const {
    return m_flag_ == ipv4_flag;
  }

  inline bool host_is_ipv6addr() const {
    return m_flag_ == ipv6_flag;
  }
};

void intrusive_ptr_add_ref(uri_private* p) {
  p->ref();
}

void intrusive_ptr_release(uri_private* p) {
  p->deref();
}

namespace {

intrusive_ptr<uri_private> m_default_uri_private(new uri_private);

} // namespace <anonymous>

} // namespace io
} // namespace caf

namespace caf {
namespace io {

optional<uri> uri::make(const string& uri_str) {
  uri_private* result = new uri_private;
  if (result->parse_uri(uri_str))
    return uri{result};
  else {
    delete result;
    return {};
  }
}

optional<uri> uri::make(const char* cstr) {
  string tmp(cstr);
  return make(tmp);
}

uri::uri() : d_(m_default_uri_private) {
  // nop
}

uri::uri(uri_private* d) : d_(d) {
  // nop
}


const string& uri::str() const {
  return d_->as_string();
}

const str_bounds& uri::host() const {
  return d_->host();
}

const str_bounds& uri::port() const {
  return d_->port();
}

uint16_t uri::port_as_int() const {
  return d_->port_as_int();
}

const str_bounds& uri::path() const {
  return d_->path();
}

const str_bounds& uri::query() const {
  return d_->query();
}

const str_bounds& uri::scheme() const {
  return d_->scheme();
}

const str_bounds& uri::fragment() const {
  return d_->fragment();
}

const str_bounds& uri::authority() const {
  return d_->authority();
}

const str_bounds& uri::user_information() const {
  return d_->user_information();
}

bool uri::host_is_ipv4addr() const {
  return d_->host_is_ipv4addr();
}

bool uri::host_is_ipv6addr() const {
  return d_->host_is_ipv6addr();
}

} // namespace io
} // namespace caf
