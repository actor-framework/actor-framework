// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/uri.hpp"

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/deserializer.hpp"
#include "caf/detail/append_hex.hpp"
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
  uri::encode(str, scheme);
  str += ':';
  if (authority.empty()) {
    CAF_ASSERT(!path.empty());
    uri::encode(str, path, true);
  } else {
    str += "//";
    str += to_string(authority);
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

// -- URI encoding -----------------------------------------------------------

void uri::encode(std::string& str, string_view x, bool is_path) {
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
      if (auto err = detail::parse(string_view{hex_buf}, val); !err) {
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
  if (!x.userinfo.empty()) {
    uri::encode(str, x.userinfo);
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
