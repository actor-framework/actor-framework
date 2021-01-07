// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/uri.hpp"

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/deserializer.hpp"
#include "caf/detail/append_percent_encoded.hpp"
#include "caf/detail/overload.hpp"
#include "caf/detail/parse.hpp"
#include "caf/detail/parser/read_uri.hpp"
#include "caf/error.hpp"
#include "caf/expected.hpp"
#include "caf/hash/fnv.hpp"
#include "caf/make_counted.hpp"
#include "caf/optional.hpp"
#include "caf/serializer.hpp"

namespace {

caf::uri::impl_type default_instance;

} // namespace

namespace caf {

uri::impl_type::impl_type() : rc_(1) {
  // nop
}

void uri::impl_type::assemble_str() {
  str.clear();
  using detail::append_percent_encoded;
  append_percent_encoded(str, scheme);
  str += ':';
  if (authority.empty()) {
    CAF_ASSERT(!path.empty());
    append_percent_encoded(str, path, true);
  } else {
    str += "//";
    str += to_string(authority);
    if (!path.empty()) {
      str += '/';
      append_percent_encoded(str, path, true);
    }
  }
  if (!query.empty()) {
    str += '?';
    auto i = query.begin();
    auto add_kvp = [&](decltype(*i) kvp) {
      append_percent_encoded(str, kvp.first);
      str += '=';
      append_percent_encoded(str, kvp.second);
    };
    add_kvp(*i);
    for (++i; i != query.end(); ++i) {
      str += '&';
      add_kvp(*i);
    }
  }
  if (!fragment.empty()) {
    str += '#';
    append_percent_encoded(str, fragment);
  }
}

uri::uri() : impl_(&default_instance) {
  // nop
}

uri::uri(impl_ptr ptr) : impl_(std::move(ptr)) {
  CAF_ASSERT(impl_ != nullptr);
}

size_t uri::hash_code() const noexcept {
  return hash::fnv<size_t>::compute(str());
}

optional<uri> uri::authority_only() const {
  if (empty() || authority().empty())
    return none;
  auto result = make_counted<uri::impl_type>();
  result->scheme = impl_->scheme;
  result->authority = impl_->authority;
  auto& str = result->str;
  str = impl_->scheme;
  str += "://";
  str += to_string(impl_->authority);
  return uri{std::move(result)};
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
