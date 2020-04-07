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

#include "caf/uri.hpp"

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/deserializer.hpp"
#include "caf/detail/append_percent_encoded.hpp"
#include "caf/detail/overload.hpp"
#include "caf/detail/parse.hpp"
#include "caf/detail/parser/read_uri.hpp"
#include "caf/detail/uri_impl.hpp"
#include "caf/error.hpp"
#include "caf/expected.hpp"
#include "caf/hash/fnv.hpp"
#include "caf/make_counted.hpp"
#include "caf/optional.hpp"
#include "caf/serializer.hpp"

namespace caf {

uri::uri() : impl_(&detail::uri_impl::default_instance) {
  // nop
}

uri::uri(impl_ptr ptr) : impl_(std::move(ptr)) {
  CAF_ASSERT(impl_ != nullptr);
}

bool uri::empty() const noexcept {
  return str().empty();
}

string_view uri::str() const noexcept {
  return impl_->str;
}

string_view uri::scheme() const noexcept {
  return impl_->scheme;
}

const uri::authority_type& uri::authority() const noexcept {
  return impl_->authority;
}

string_view uri::path() const noexcept {
  return impl_->path;
}

const uri::query_map& uri::query() const noexcept {
  return impl_->query;
}

string_view uri::fragment() const noexcept {
  return impl_->fragment;
}

size_t uri::hash_code() const noexcept {
  return hash::fnv<size_t>::compute(str());
}

optional<uri> uri::authority_only() const {
  if (empty() || authority().empty())
    return none;
  auto result = make_counted<detail::uri_impl>();
  result->scheme = impl_->scheme;
  result->authority = impl_->authority;
  auto& str = result->str;
  str = impl_->scheme;
  str += "://";
  str += to_string(impl_->authority);
  return uri{std::move(result)};
}

// -- comparison ---------------------------------------------------------------

int uri::compare(const uri& other) const noexcept {
  return str().compare(other.str());
}

int uri::compare(string_view x) const noexcept {
  return string_view{str()}.compare(x);
}

// -- parsing ------------------------------------------------------------------

namespace {

class nop_builder {
public:
  template <class T>
  nop_builder& scheme(T&&) {
    return *this;
  }

  template <class T>
  nop_builder& userinfo(T&&) {
    return *this;
  }

  template <class T>
  nop_builder& host(T&&) {
    return *this;
  }

  template <class T>
  nop_builder& port(T&&) {
    return *this;
  }

  template <class T>
  nop_builder& path(T&&) {
    return *this;
  }

  template <class T>
  nop_builder& query(T&&) {
    return *this;
  }

  template <class T>
  nop_builder& fragment(T&&) {
    return *this;
  }
};

} // namespace

bool uri::can_parse(string_view str) noexcept {
  string_parser_state ps{str.begin(), str.end()};
  nop_builder builder;
  if (ps.consume('<')) {
    detail::parser::read_uri(ps, builder);
    if (ps.code > pec::trailing_character)
      return false;
    if (!ps.consume('>'))
      return false;
  } else {
    detail::parser::read_uri(ps, builder);
  }
  return ps.code == pec::success;
}

// -- friend functions ---------------------------------------------------------

error inspect(caf::serializer& dst, uri& x) {
  return inspect(dst, const_cast<detail::uri_impl&>(*x.impl_));
}

error inspect(caf::deserializer& src, uri& x) {
  auto impl = make_counted<detail::uri_impl>();
  auto err = inspect(src, *impl);
  if (err == none)
    x = uri{std::move(impl)};
  return err;
}

error_code<sec> inspect(caf::binary_serializer& dst, uri& x) {
  return inspect(dst, const_cast<detail::uri_impl&>(*x.impl_));
}

error_code<sec> inspect(caf::binary_deserializer& src, uri& x) {
  auto impl = make_counted<detail::uri_impl>();
  auto err = inspect(src, *impl);
  if (err == none)
    x = uri{std::move(impl)};
  return err;
}

// -- related free functions ---------------------------------------------------

std::string to_string(const uri& x) {
  auto x_str = x.str();
  std::string result{x_str.begin(), x_str.end()};
  return result;
}

std::string to_string(const uri::authority_type& x) {
  std::string str;
  if (!x.userinfo.empty()) {
    detail::append_percent_encoded(str, x.userinfo);
    str += '@';
  }
  auto f = caf::detail::make_overload(
    [&](const ip_address& addr) {
      if (addr.embeds_v4()) {
        str += to_string(addr);
      } else {
        str += '[';
        str += to_string(addr);
        str += ']';
      }
    },
    [&](const std::string& host) {
      detail::append_percent_encoded(str, host);
    });
  visit(f, x.host);
  if (x.port != 0) {
    str += ':';
    str += std::to_string(x.port);
  }
  return str;
}

error parse(string_view str, uri& dest) {
  string_parser_state ps{str.begin(), str.end()};
  parse(ps, dest);
  if (ps.code == pec::success)
    return none;
  return make_error(ps);
}

expected<uri> make_uri(string_view str) {
  uri result;
  if (auto err = parse(str, result))
    return err;
  return result;
}

} // namespace caf
