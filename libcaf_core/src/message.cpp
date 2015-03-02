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

#include "caf/message.hpp"

#include <iostream>

#include "caf/message_handler.hpp"
#include "caf/string_algorithms.hpp"

#include "caf/detail/singletons.hpp"
#include "caf/detail/decorated_tuple.hpp"
#include "caf/detail/concatenated_tuple.hpp"

namespace caf {

message::message(detail::message_data* ptr) : m_vals(ptr) {
  // nop
}

message::message(message&& other) : m_vals(std::move(other.m_vals)) {
  // nop
}

message::message(const data_ptr& ptr) : m_vals(ptr) {
  // nop
}

message& message::operator=(message&& other) {
  m_vals.swap(other.m_vals);
  return *this;
}

void message::reset(raw_ptr new_ptr) {
  m_vals.reset(new_ptr);
}

void message::swap(message& other) {
  m_vals.swap(other.m_vals);
}

void* message::mutable_at(size_t p) {
  CAF_REQUIRE(m_vals);
  return m_vals->mutable_at(p);
}

const void* message::at(size_t p) const {
  CAF_REQUIRE(m_vals);
  return m_vals->at(p);
}

bool message::match_element(size_t pos, uint16_t typenr,
                            const std::type_info* rtti) const {
  return m_vals->match_element(pos, typenr, rtti);
}

const char* message::uniform_name_at(size_t pos) const {
  return m_vals->uniform_name_at(pos);
}

bool message::equals(const message& other) const {
  CAF_REQUIRE(m_vals);
  return m_vals->equals(*other.vals());
}

message message::drop(size_t n) const {
  CAF_REQUIRE(m_vals);
  if (n == 0) {
    return *this;
  }
  if (n >= size()) {
    return message{};
  }
  std::vector<size_t> mapping (size() - n);
  size_t i = n;
  std::generate(mapping.begin(), mapping.end(), [&] { return i++; });
  return message {detail::decorated_tuple::make(m_vals, mapping)};
}

message message::drop_right(size_t n) const {
  CAF_REQUIRE(m_vals);
  if (n == 0) {
    return *this;
  }
  if (n >= size()) {
    return message{};
  }
  std::vector<size_t> mapping(size() - n);
  std::iota(mapping.begin(), mapping.end(), size_t{0});
  return message{detail::decorated_tuple::make(m_vals, std::move(mapping))};
}

message message::slice(size_t pos, size_t n) const {
  auto s = size();
  if (pos >= s) {
    return message{};
  }
  std::vector<size_t> mapping(std::min(s - pos, n));
  std::iota(mapping.begin(), mapping.end(), pos);
  return message{detail::decorated_tuple::make(m_vals, std::move(mapping))};
}

optional<message> message::apply(message_handler handler) {
  return handler(*this);
}

message message::extract_impl(size_t start, message_handler handler) const {
  auto s = size();
  for (size_t i = start; i < s; ++i) {
    for (size_t n = (s - i) ; n > 0; --n) {
      auto res = handler(slice(i, n));
      if (res) {
        std::vector<size_t> mapping(s);
        std::iota(mapping.begin(), mapping.end(), size_t{0});
        auto first = mapping.begin() + static_cast<ptrdiff_t>(i);
        auto last = first + static_cast<ptrdiff_t>(n);
        mapping.erase(first, last);
        if (mapping.empty()) {
          return message{};
        }
        message next{detail::decorated_tuple::make(m_vals, std::move(mapping))};
        return next.extract_impl(i, handler);
      }
    }
  }
  return *this;
}

message message::extract(message_handler handler) const {
  return extract_impl(0, handler);
}

message::cli_res message::extract_opts(std::vector<cli_arg> cliargs) const {
  std::set<std::string> opts;
  cli_arg dummy{"help,h", ""};
  std::map<std::string, cli_arg*> shorts;
  std::map<std::string, cli_arg*> longs;
  shorts["-h"] = &dummy;
  shorts["-?"] = &dummy;
  longs["--help"] = &dummy;
  for (auto& cliarg : cliargs) {
    std::vector<std::string> s;
    split(s, cliarg.name, is_any_of(","), token_compress_on);
    if (s.size() == 2 && s.back().size() == 1) {
      longs["--" + s.front()] = &cliarg;
      shorts["-" + s.back()] = &cliarg;
    } else if (s.size() == 1) {
      longs[s.front()] = &cliarg;
    } else {
      throw std::invalid_argument("invalid option name: " + cliarg.name);
    }
  }
  auto insert_opt_name = [&](const cli_arg* ptr) {
    auto separator = ptr->name.find(',');
    if (separator == std::string::npos) {
      opts.insert(ptr->name);
    } else {
      opts.insert(ptr->name.substr(0, separator));
    }
  };
  auto res = extract({
    [&](const std::string& arg) -> optional<skip_message_t> {
      if (arg.empty() || arg.front() != '-') {
        return skip_message();
      }
      auto i = shorts.find(arg);
      if (i != shorts.end()) {
        if (i->second->fun) {
          return skip_message();
        }
        insert_opt_name(i->second);
        return none;
      }
      auto eq_pos = arg.find('=');
      auto j = longs.find(arg.substr(0, eq_pos));
      if (j != longs.end()) {
        if (j->second->fun) {
          if (eq_pos == std::string::npos) {
            std::cerr << "missing argument to " << arg << std::endl;
            return none;
          }
          if (!j->second->fun(arg.substr(eq_pos + 1))) {
            std::cerr << "invalid value for option "
                      << j->second->name << ": " << arg << std::endl;
            return none;
          }
          insert_opt_name(j->second);
          return none;
        }
        insert_opt_name(j->second);
        return none;
      }
      std::cerr << "unknown command line option: " << arg << std::endl;
      return none;
    },
    [&](const std::string& arg1,
        const std::string& arg2) -> optional<skip_message_t> {
      if (arg1.size() < 2 || arg1[0] != '-' || arg1[1] == '-') {
        return skip_message();
      }
      auto i = shorts.find(arg1);
      if (i != shorts.end() && i->second->fun) {
        if (!i->second->fun(arg2)) {
          std::cerr << "invalid value for option "
                    << i->second->name << ": " << arg2 << std::endl;
          return none;
        }
        insert_opt_name(i->second);
        return none;
      }
      std::cerr << "unknown command line option: " << arg1 << std::endl;
      return none;
    }
  });
  size_t name_width = 0;
  for (auto& ca : cliargs) {
    // name field contains either only "--<long_name>" or
    // "-<short name> [--<long name>]" depending on whether or not
    // a ',' appears in the name
    auto nw = ca.name.find(',') == std::string::npos
              ? ca.name.size() + 2  // "--<name>"
              : ca.name.size() + 5; // "-X [--<name>]" (minus trailing ",X")
    if (ca.fun) {
      nw += 4; // trailing " arg"
    }
    name_width = std::max(name_width, nw);
  }
  std::ostringstream oss;
  oss << std::left;
  oss << "Allowed options:" << std::endl;
  for (auto& ca : cliargs) {
    std::string lhs;
    auto separator = ca.name.find(',');
    if (separator == std::string::npos) {
      lhs += "--";
      lhs += ca.name;
    } else {
      lhs += "-";
      lhs += ca.name.back();
      lhs += " [--";
      lhs += ca.name.substr(0, separator);
      lhs += "]";
    }
    if (ca.fun) {
      lhs += " arg";
    }
    oss << "  ";
    oss.width(static_cast<std::streamsize>(name_width));
    oss << lhs << "  : " << ca.text << std::endl;
  }
  auto helptext = oss.str();
  if (opts.count("help") == 1) {
    std::cout << helptext << std::endl;
  }
  return {res, std::move(opts), std::move(helptext)};
}

message::cli_arg::cli_arg(std::string nstr, std::string tstr)
    : name(std::move(nstr)),
      text(std::move(tstr)) {
  // nop
}

message::cli_arg::cli_arg(std::string nstr, std::string tstr, std::string& arg)
    : name(std::move(nstr)),
      text(std::move(tstr)) {
  fun = [&arg](const std::string& str) -> bool {
    arg = str;
    return true;
  };
}

message::cli_arg::cli_arg(std::string nstr, std::string tstr,
                          std::vector<std::string>& arg)
    : name(std::move(nstr)), text(std::move(tstr)) {
  fun = [&arg](const std::string& str) -> bool {
    arg.push_back(str);
    return true;
  };
}

message message::concat_impl(std::initializer_list<data_ptr> xs) {
  auto not_nullptr = [](const data_ptr& ptr) { return ptr.get() != nullptr; };
  switch (std::count_if(xs.begin(), xs.end(), not_nullptr)) {
    case 0:
      return message{};
    case 1:
      return message{*std::find_if(xs.begin(), xs.end(), not_nullptr)};
    default:
      return message{detail::concatenated_tuple::make(xs)};
  }
}

} // namespace caf
