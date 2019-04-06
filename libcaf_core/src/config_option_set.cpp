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

#include "caf/config_option_set.hpp"

#include <map>
#include <set>

#include "caf/config_option.hpp"
#include "caf/config_value.hpp"
#include "caf/detail/algorithms.hpp"
#include "caf/expected.hpp"

using std::string;

namespace {

struct string_builder {
  std::string result;
};

string_builder& operator<<(string_builder& builder, char ch) {
  builder.result += ch;
  return builder;
}

string_builder& operator<<(string_builder& builder, caf::string_view str) {
  builder.result.append(str.data(), str.size());
  return builder;
}

void insert(string_builder& builder, size_t count, char ch) {
  builder.result.insert(builder.result.end(), count, ch);
}

} // namespace <anonymous>

namespace caf {

config_option_set& config_option_set::add(config_option opt) {
  opts_.emplace_back(std::move(opt));
  return *this;
}

std::string config_option_set::help_text(bool global_only) const {
  //<--- argument --------> <---- desciption ---->
  // (-w|--write) <string> : output file
  auto build_argument = [](const config_option& x) {
    string_builder sb;
    if (x.short_names().empty()) {
      sb << "  --";
      if (!x.has_flat_cli_name())
        sb << x.category() << '.';
      sb << x.long_name();
      if (!x.is_flag())
        sb << '=';
    } else {
      sb << "  (";
      for (auto c : x.short_names())
        sb << '-' << c << '|';
      sb << "--";
      if (!x.has_flat_cli_name())
        sb << x.category() << '.';
      sb << x.long_name() << ") ";
    }
    if (!x.is_flag())
      sb << "<" << x.type_name() << '>';
    return std::move(sb.result);
  };
  // Sort argument + description by category.
  using pair = std::pair<std::string, option_pointer>;
  std::set<string_view> categories;
  std::multimap<string_view, pair> args;
  size_t max_arg_size = 0;
  for (auto& opt : opts_) {
    // We treat all options with flat name as-if the category was 'global'.
    if (!global_only || opt.has_flat_cli_name()) {
      auto arg = build_argument(opt);
      max_arg_size = std::max(max_arg_size, arg.size());
      string_view category = "global";
      if (!opt.has_flat_cli_name())
        category = opt.category();
      categories.emplace(category);
      args.emplace(category, std::make_pair(std::move(arg), &opt));
    }
  }
  // Build help text by iterating over all categories in the multimap.
  string_builder builder;
  for (auto& category : categories) {
    auto rng = args.equal_range(category);
    builder << category << " options:\n";
    for (auto i = rng.first; i != rng.second; ++i) {
      builder << i->second.first;
      CAF_ASSERT(max_arg_size >= i->second.first.size());
      insert(builder, max_arg_size - i->second.first.size(), ' ');
      builder << " : " << i->second.second->description() << '\n';
    }
    builder << '\n';
  }
  return std::move(builder.result);
}

namespace {

settings& select_entry(settings& config, string_view key){
  auto sep = key.find('.');
  if (sep == string_view::npos)
    return config[key].as_dictionary();
  auto prefix = key.substr(0, sep);
  auto suffix = key.substr(sep + 1);
  return select_entry(config[prefix].as_dictionary(), suffix);
}

} // namespace

auto config_option_set::parse(settings& config, argument_iterator first,
                              argument_iterator last) const
  -> std::pair<pec, argument_iterator> {
  // Sanity check.
  if (first == last)
    return {pec::success, last};
  // Parses an argument.
  using iter = string::const_iterator;
  auto consume = [&](const config_option& opt, iter arg_begin, iter arg_end) {
    // Extract option name and category.
    auto opt_name = opt.long_name();
    auto opt_ctg = opt.category();
    // Try inserting a new submap into the config or fill existing one.
    auto& entry = opt_ctg == "global" ? config : select_entry(config, opt_ctg);
    // Flags only consume the current element.
    if (opt.is_flag()) {
      if (arg_begin != arg_end)
        return pec::illegal_argument;
      config_value cfg_true{true};
      opt.store(cfg_true);
      entry[opt_name] = cfg_true;
    } else {
      if (arg_begin == arg_end)
        return pec::missing_argument;
      auto slice_size = static_cast<size_t>(std::distance(arg_begin, arg_end));
      string_view slice{&*arg_begin, slice_size};
      auto val = opt.parse(slice);
      if (!val) {
        auto& err = val.error();
        if (err.category() == atom("parser"))
          return static_cast<pec>(err.code());
        return pec::illegal_argument;
      }
      entry[opt_name] = std::move(*val);
    }
    return pec::success;
  };
  // We loop over the first N-1 values, because we always consider two
  // arguments at once.
  for (auto i =  first; i != last;) {
    if (i->size() < 2)
      return {pec::not_an_option, i};
    if (*i== "--")
      return {pec::success, std::next(first)};
    if (i->compare(0, 2, "--") == 0) {
      // Long options use the syntax "--<name>=<value>" and consume only a
      // single argument.
      auto npos = std::string::npos;
      auto assign_op = i->find('=');
      auto name = assign_op == npos ? i->substr(2)
                                    : i->substr(2, assign_op - 2);
      auto opt = cli_long_name_lookup(name);
      if (opt == nullptr)
        return {pec::not_an_option, i};
      auto code = consume(*opt,
                          assign_op == npos
                          ? i->end()
                          : i->begin() + static_cast<ptrdiff_t>(assign_op + 1),
                          i->end());
      if (code != pec::success)
        return {code, i};
      ++i;
    } else if (i->front() == '-') {
      // Short options have three possibilities.
      auto opt = cli_short_name_lookup((*i)[1]);
      if (opt == nullptr)
        return {pec::not_an_option, i};
      if (opt->is_flag()) {
        // 1) "-f" for flags, consumes one argument
        auto code = consume(*opt, i->begin() + 2, i->end());
        if (code != pec::success)
          return {code, i};
        ++i;
      } else {
        if (i->size() == 2) {
          // 2) "-k <value>", consumes both arguments
          auto j = std::next(i);
          if (j == last) {
            return {pec::missing_argument, j};
          }
          auto code = consume(*opt, j->begin(), j->end());
          if (code != pec::success)
            return {code, i};
          std::advance(i, 2);
        } else {
          // 3) "-k<value>" (no space), consumes one argument
          auto code = consume(*opt, i->begin() + 2, i->end());
          if (code != pec::success)
            return {code, i};
          ++i;
        }
      }
    } else {
      // No leading '-' found on current position.
      return {pec::not_an_option, i};
    }
  }
  return {pec::success, last};
}

config_option_set::parse_result
config_option_set::parse(settings& config,
                         const std::vector<string>& args) const {
  return parse(config, args.begin(), args.end());
}

config_option_set::option_pointer
config_option_set::cli_long_name_lookup(string_view input) const {
  // We accept "caf#" prefixes for backward compatibility, but ignore them.
  auto name = input.substr(input.compare(0, 4, "caf#") != 0 ? 0u : 4u);
  // Extract category and long name.
  string_view category;
  string_view long_name;
  auto sep = name.find_last_of('.');
  if (sep == string::npos) {
    long_name = name;
  } else {
    category = name.substr(0, sep);
    long_name = name.substr(sep + 1);
  }
  // Scan all options for a match.
  auto category_match = [&](const config_option& opt) {
    return sep == string::npos ? opt.has_flat_cli_name()
                               : opt.category() == category;
  };
  return detail::ptr_find_if(opts_, [&](const config_option& opt) {
    return category_match(opt) && opt.long_name() == long_name;
  });
}

config_option_set::option_pointer
config_option_set::cli_short_name_lookup(char short_name) const {
  return detail::ptr_find_if(opts_, [&](const config_option& opt) {
    return opt.short_names().find(short_name) != string::npos;
  });
}

config_option_set::option_pointer
config_option_set::qualified_name_lookup(string_view category,
                                         string_view long_name) const {
  return detail::ptr_find_if(opts_, [&](const config_option& opt) {
    return opt.category() == category && opt.long_name() == long_name;
  });
}

config_option_set::option_pointer
config_option_set::qualified_name_lookup(string_view name) const {
  auto sep = name.find('.');
  if (sep == string::npos)
    return nullptr;
  return qualified_name_lookup(name.substr(0, sep), name.substr(sep + 1));
}

} // namespace caf
