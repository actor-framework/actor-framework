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

#include "caf/deserializer.hpp"
#include "caf/detail/parser/read_uri.hpp"
#include "caf/detail/uri_impl.hpp"
#include "caf/error.hpp"
#include "caf/make_counted.hpp"
#include "caf/serializer.hpp"
#include "caf/uri_builder.hpp"

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

// -- comparison ---------------------------------------------------------------

int uri::compare(const uri& other) const noexcept {
  return str().compare(other.str());
}

int uri::compare(string_view x) const noexcept {
  return string_view{str()}.compare(x);
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

// -- related free functions ---------------------------------------------------

std::string to_string(const uri& x) {
  auto x_str = x.str();
  std::string result{x_str.begin(), x_str.end()};
  return result;
}

error parse(string_view str, uri& dest) {
  using namespace detail::parser;
  uri_builder builder;
  state<string_view::iterator> res{str.begin(), str.end()};
  read_uri(res, builder);
  if (res.code == pec::success) {
    dest = builder.make();
    return none;
  }
  return make_error(res.code, static_cast<size_t>(res.line),
                    static_cast<size_t>(res.column));
}

} // namespace caf
