// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/uri.hpp"

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/deserializer.hpp"
#include "caf/detail/append_hex.hpp"
#include "caf/detail/assert.hpp"
#include "caf/detail/overload.hpp"
#include "caf/detail/parse.hpp"
#include "caf/detail/parser/read_uri.hpp"
#include "caf/error.hpp"
#include "caf/expected.hpp"
#include "caf/hash/fnv.hpp"
#include "caf/make_counted.hpp"
#include "caf/serializer.hpp"

#include <optional>

namespace {

caf::uri::impl_type default_instance;

} // namespace

namespace caf {

std::string uri::authority_type::host_str() const {
  if (auto* str = std::get_if<std::string>(&host))
    return *str;
  return to_string(std::get<ip_address>(host));
}

uri::impl_type::impl_type() : rc_(1) {
  // nop
}

void uri::impl_type::assemble_str() {
  str.clear();
  uri::encode(str, scheme);
  str += ':';
  if (authority.empty()) {
    CAF_ASSERT(!path.empty());
    path_offset = str.size();
    uri::encode(str, path, true);
  } else {
    str += "//";
    str += to_string(authority);
    path_offset = str.size();
    if (!path.empty()) {
      str += '/';
      uri::encode(str, path, true);
    }
  }
  if (!query.empty()) {
    str += '?';
    auto i = query.begin();
    auto add_kvp = [&](decltype(*i) kvp) {
      uri::encode(str, kvp.first);
      str += '=';
      uri::encode(str, kvp.second);
    };
    add_kvp(*i);
    for (++i; i != query.end(); ++i) {
      str += '&';
      add_kvp(*i);
    }
  }
  if (!fragment.empty()) {
    str += '#';
    uri::encode(str, fragment);
  }
}

uri::uri() : impl_(&default_instance) {
  // nop
}

uri::uri(impl_ptr ptr) : impl_(std::move(ptr)) {
  CAF_ASSERT(impl_ != nullptr);
}

std::string uri::host_str() const {
  const auto& host = impl_->authority.host;
  if (std::holds_alternative<std::string>(host)) {
    return std::get<std::string>(host);
  } else {
    return to_string(std::get<ip_address>(host));
  }
}

std::string uri::path_query_fragment() const {
  std::string result;
  auto sub_str = impl_->str_after_path_offset();
  if (sub_str.empty() || sub_str[0] != '/') {
    result += '/';
  }
  result.insert(result.begin(), sub_str.begin(), sub_str.end());
  return result;
}

size_t uri::hash_code() const noexcept {
  return hash::fnv<size_t>::compute(str());
}

std::optional<uri> uri::authority_only() const {
  if (empty() || authority().empty())
    return std::nullopt;
  auto result = make_counted<uri::impl_type>();
  result->scheme = impl_->scheme;
  result->authority = impl_->authority;
  auto& str = result->str;
  str = impl_->scheme;
  str += "://";
  str += to_string(impl_->authority);
  return uri{std::move(result)};
}

namespace {

template <class Password>
uri::impl_ptr with_userinfo_impl(const uri::impl_ptr& src, std::string&& name,
                                 Password&& password) {
  uri::userinfo_type userinfo{std::move(name),
                              std::forward<Password>(password)};
  auto result = make_counted<uri::impl_type>();
  result->scheme = src->scheme;
  result->authority = src->authority;
  result->authority.userinfo.emplace(std::move(userinfo));
  result->path = src->path;
  result->query = src->query;
  result->fragment = src->fragment;
  result->assemble_str();
  return result;
}

} // namespace

std::optional<uri> uri::with_userinfo(std::string name) const {
  if (empty() || authority().empty()) {
    return std::nullopt;
  }
  auto result = with_userinfo_impl(impl_, std::move(name), std::nullopt);
  return uri{std::move(result)};
}

std::optional<uri> uri::with_userinfo(std::string name,
                                      std::string password) const {
  if (empty() || authority().empty()) {
    return std::nullopt;
  }
  auto result = with_userinfo_impl(impl_, std::move(name), std::move(password));
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

  template <class... Ts>
  nop_builder& userinfo(Ts&&...) {
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

bool uri::can_parse(std::string_view str) noexcept {
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

// -- URI encoding -----------------------------------------------------------

void uri::encode(std::string& str, std::string_view x, bool is_path) {
  for (auto ch : x)
    switch (ch) {
      case ':':
      case '/':
        if (is_path) {
          str += ch;
          break;
        }
        [[fallthrough]];
      case ' ':
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
        detail::append_hex(str, ch);
        break;
      default:
        str += ch;
    }
}

void uri::decode(std::string& str) {
  // Buffer for holding temporary strings and variable for parsing into.
  char str_buf[2] = {' ', '\0'};
  char hex_buf[5] = {'0', 'x', '0', '0', '\0'};
  uint8_t val = 0;
  // Any percent-encoded string must have at least 3 characters.
  if (str.size() < 3)
    return;
  // Iterate over the string to find '%XX' entries and replace them.
  for (size_t index = 0; index < str.size() - 2; ++index) {
    if (str[index] == '%') {
      hex_buf[2] = str[index + 1];
      hex_buf[3] = str[index + 2];
      if (auto err = detail::parse(std::string_view{hex_buf}, val); !err) {
        str_buf[0] = static_cast<char>(val);
        str.replace(index, 3, str_buf, 1);
      } else {
        str.replace(index, 3, "?", 1);
      }
    }
  }
}

// -- related free functions ---------------------------------------------------

std::string to_string(const uri& x) {
  auto x_str = x.str();
  std::string result{x_str.begin(), x_str.end()};
  return result;
}

std::string to_string(const uri::authority_type& x) {
  std::string str;
  if (x.userinfo) {
    uri::encode(str, x.userinfo->name);
    if (auto& pw = x.userinfo->password) {
      str += ':';
      uri::encode(str, *pw);
    }
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
    [&](const std::string& host) { uri::encode(str, host); });
  visit(f, x.host);
  if (x.port != 0) {
    str += ':';
    str += std::to_string(x.port);
  }
  return str;
}

error parse(std::string_view str, uri& dest) {
  string_parser_state ps{str.begin(), str.end()};
  parse(ps, dest);
  if (ps.code == pec::success)
    return none;
  return ps.error();
}

expected<uri> make_uri(std::string_view str) {
  uri result;
  if (auto err = parse(str, result))
    return err;
  return result;
}

} // namespace caf
