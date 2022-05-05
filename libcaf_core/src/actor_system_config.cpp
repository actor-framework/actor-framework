// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/actor_system_config.hpp"

#include <fstream>
#include <limits>
#include <sstream>
#include <thread>

#include "caf/config.hpp"
#include "caf/config_option.hpp"
#include "caf/config_option_adder.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/config_consumer.hpp"
#include "caf/detail/gcd.hpp"
#include "caf/detail/parser/read_config.hpp"
#include "caf/detail/parser/read_string.hpp"
#include "caf/message_builder.hpp"
#include "caf/pec.hpp"
#include "caf/sec.hpp"
#include "caf/type_id.hpp"

namespace caf {

namespace {

constexpr const char* default_config_file = "caf-application.conf";

} // namespace

actor_system_config::~actor_system_config() {
  // nop
}

// in this config class, we have (1) hard-coded defaults that are overridden
// by (2) config file contents that are in turn overridden by (3) CLI arguments

actor_system_config::actor_system_config()
  : cli_helptext_printed(false),
    program_name("unknown-caf-app"),
    slave_mode(false),
    config_file_path(default_config_file),
    slave_mode_fun(nullptr) {
  // fill our options vector for creating config file and CLI parsers
  using std::string;
  using string_list = std::vector<string>;
  opt_group{custom_options_, "global"}
    .add<bool>("help,h?", "print help text to STDERR and exit")
    .add<bool>("long-help", "print long help text to STDERR and exit")
    .add<bool>("dump-config", "print configuration to STDERR and exit")
    .add<string>("config-file", "sets a path to a configuration file");
  opt_group{custom_options_, "caf.scheduler"}
    .add<string>("policy", "'stealing' (default) or 'sharing'")
    .add<size_t>("max-threads", "maximum number of worker threads")
    .add<size_t>("max-throughput", "nr. of messages actors can consume per run")
    .add<bool>("enable-profiling", "enables profiler output")
    .add<timespan>("profiling-resolution", "data collection rate")
    .add<string>("profiling-output-file", "output file for the profiler");
  opt_group(custom_options_, "caf.work-stealing")
    .add<size_t>("aggressive-poll-attempts", "nr. of aggressive steal attempts")
    .add<size_t>("aggressive-steal-interval",
                 "frequency of aggressive steal attempts")
    .add<size_t>("moderate-poll-attempts", "nr. of moderate steal attempts")
    .add<size_t>("moderate-steal-interval",
                 "frequency of moderate steal attempts")
    .add<timespan>("moderate-sleep-duration",
                   "sleep duration between moderate steal attempts")
    .add<size_t>("relaxed-steal-interval",
                 "frequency of relaxed steal attempts")
    .add<timespan>("relaxed-sleep-duration",
                   "sleep duration between relaxed steal attempts");
  opt_group{custom_options_, "caf.logger"} //
    .add<bool>("inline-output", "disable logger thread (for testing only!)");
  opt_group{custom_options_, "caf.logger.file"}
    .add<string>("path", "filesystem path for the log file")
    .add<string>("format", "format for individual log file entries")
    .add<string>("verbosity", "minimum severity level for file output")
    .add<string_list>("excluded-components", "excluded components in files");
  opt_group{custom_options_, "caf.logger.console"}
    .add<bool>("colored", "forces colored or uncolored output")
    .add<string>("format", "format for printed console lines")
    .add<string>("verbosity", "minimum severity level for console output")
    .add<string_list>("excluded-components", "excluded components on console");
  opt_group{custom_options_, "caf.metrics-filters.actors"}
    .add<string_list>("includes", "selects actors for run-time metrics")
    .add<string_list>("excludes", "excludes actors from run-time metrics");
}

settings actor_system_config::dump_content() const {
  settings result = content;
  auto& caf_group = result["caf"].as_dictionary();
  // -- scheduler parameters
  auto& scheduler_group = caf_group["scheduler"].as_dictionary();
  put_missing(scheduler_group, "policy", defaults::scheduler::policy);
  put_missing(scheduler_group, "max-throughput",
              defaults::scheduler::max_throughput);
  put_missing(scheduler_group, "enable-profiling", false);
  put_missing(scheduler_group, "profiling-resolution",
              defaults::scheduler::profiling_resolution);
  put_missing(scheduler_group, "profiling-output-file", std::string{});
  // -- work-stealing parameters
  auto& work_stealing_group = caf_group["work-stealing"].as_dictionary();
  put_missing(work_stealing_group, "aggressive-poll-attempts",
              defaults::work_stealing::aggressive_poll_attempts);
  put_missing(work_stealing_group, "aggressive-steal-interval",
              defaults::work_stealing::aggressive_steal_interval);
  put_missing(work_stealing_group, "moderate-poll-attempts",
              defaults::work_stealing::moderate_poll_attempts);
  put_missing(work_stealing_group, "moderate-steal-interval",
              defaults::work_stealing::moderate_steal_interval);
  put_missing(work_stealing_group, "moderate-sleep-duration",
              defaults::work_stealing::moderate_sleep_duration);
  put_missing(work_stealing_group, "relaxed-steal-interval",
              defaults::work_stealing::relaxed_steal_interval);
  put_missing(work_stealing_group, "relaxed-sleep-duration",
              defaults::work_stealing::relaxed_sleep_duration);
  // -- logger parameters
  auto& logger_group = caf_group["logger"].as_dictionary();
  put_missing(logger_group, "inline-output", false);
  auto& file_group = logger_group["file"].as_dictionary();
  put_missing(file_group, "path", defaults::logger::file::path);
  put_missing(file_group, "format", defaults::logger::file::format);
  put_missing(file_group, "excluded-components", std::vector<std::string>{});
  auto& console_group = logger_group["console"].as_dictionary();
  put_missing(console_group, "colored", defaults::logger::console::colored);
  put_missing(console_group, "format", defaults::logger::console::format);
  put_missing(console_group, "excluded-components", std::vector<std::string>{});
  // -- middleman parameters
  auto& middleman_group = caf_group["middleman"].as_dictionary();
  auto default_id = std::string{defaults::middleman::app_identifier};
  put_missing(middleman_group, "app-identifiers",
              std::vector<std::string>{std::move(default_id)});
  put_missing(middleman_group, "enable-automatic-connections", false);
  put_missing(middleman_group, "max-consecutive-reads",
              defaults::middleman::max_consecutive_reads);
  put_missing(middleman_group, "heartbeat-interval",
              defaults::middleman::heartbeat_interval);
  // -- openssl parameters
  auto& openssl_group = caf_group["openssl"].as_dictionary();
  put_missing(openssl_group, "certificate", std::string{});
  put_missing(openssl_group, "key", std::string{});
  put_missing(openssl_group, "passphrase", std::string{});
  put_missing(openssl_group, "capath", std::string{});
  put_missing(openssl_group, "cafile", std::string{});
  return result;
}

error actor_system_config::parse(int argc, char** argv) {
  string_list args;
  if (argc > 0) {
    program_name = argv[0];
    if (argc > 1)
      args.assign(argv + 1, argv + argc);
  }
  return parse(std::move(args));
}

error actor_system_config::parse(int argc, char** argv, std::istream& conf) {
  string_list args;
  if (argc > 0) {
    program_name = argv[0];
    if (argc > 1)
      args.assign(argv + 1, argv + argc);
  }
  return parse(std::move(args), conf);
}

std::pair<int, char**> actor_system_config::c_args_remainder() const noexcept {
  return {static_cast<int>(c_args_remainder_.size()), c_args_remainder_.data()};
}

void actor_system_config::set_remainder(string_list args) {
  remainder.swap(args);
  c_args_remainder_buf_.assign(program_name.begin(), program_name.end());
  c_args_remainder_buf_.emplace_back('\0');
  for (const auto& arg : remainder) {
    c_args_remainder_buf_.insert(c_args_remainder_buf_.end(), //
                                 arg.begin(), arg.end());
    c_args_remainder_buf_.emplace_back('\0');
  }
  auto ptr = c_args_remainder_buf_.data();
  auto end = ptr + c_args_remainder_buf_.size();
  auto advance_ptr = [&ptr] {
    while (*ptr++ != '\0')
      ; // nop
  };
  for (; ptr != end; advance_ptr())
    c_args_remainder_.emplace_back(ptr);
}

namespace {

struct config_iter {
  std::istream* conf;
  char ch;

  explicit config_iter(std::istream* istr) : conf(istr) {
    conf->get(ch);
  }

  config_iter() : conf(nullptr), ch('\0') {
    // nop
  }

  config_iter(const config_iter&) = default;

  config_iter& operator=(const config_iter&) = default;

  inline char operator*() const {
    return ch;
  }

  inline config_iter& operator++() {
    conf->get(ch);
    return *this;
  }
};

struct config_sentinel {};

bool operator!=(config_iter iter, config_sentinel) {
  return !iter.conf->fail();
}

struct indentation {
  size_t size;
};

indentation operator+(indentation x, size_t y) noexcept {
  return {x.size + y};
}

std::ostream& operator<<(std::ostream& out, indentation indent) {
  for (size_t i = 0; i < indent.size; ++i)
    out.put(' ');
  return out;
}

// Fakes a buffer interface but really prints to std::cout.
struct out_buf {
  int end() {
    return 0;
  }
  void push_back(char c) {
    std::cout << c;
  }
  template <class Iterator, class Sentinel>
  void insert(int, Iterator i, Sentinel e) {
    while (i != e)
      std::cout << *i++;
  }
};

void print(const config_value::dictionary& xs, indentation indent);

void print_val(const config_value& val, indentation indent) {
  out_buf out;
  using std::cout;
  switch (val.get_data().index()) {
    default: // none
      // omit
      break;
    case 1: // integer
      detail::print(out, get<config_value::integer>(val));
      break;
    case 2: // boolean
      detail::print(out, get<config_value::boolean>(val));
      break;
    case 3: // real
      detail::print(out, get<config_value::real>(val));
      break;
    case 4: // timespan
      detail::print(out, get<timespan>(val));
      break;
    case 5: // uri
      cout << '<' << get<uri>(val).str() << '>';
      break;
    case 6: // string
      detail::print_escaped(out, get<std::string>(val));
      break;
    case 7: { // list
      auto& xs = get<config_value::list>(val);
      if (xs.empty()) {
        cout << "[]";
      } else {
        auto list_indent = indent + 2;
        cout << "[\n";
        for (auto& x : xs) {
          cout << list_indent;
          print_val(x, list_indent);
          cout << ",\n";
        }
        cout << indent << ']';
      }
      break;
    }
    case 8: { // dictionary
      print(get<config_value::dictionary>(val), indent + 2);
    }
  }
}

bool needs_quotes(const std::string& key) {
  auto is_alnum_or_dash = [](char x) {
    return isalnum(x) || x == '-' || x == '_';
  };
  return key.empty() || !std::all_of(key.begin(), key.end(), is_alnum_or_dash);
}

void print(const config_value::dictionary& xs, indentation indent) {
  out_buf out;
  using std::cout;
  bool top_level = indent.size == 0;
  for (const auto& [key, val] : xs) {
    if (!top_level || (key != "dump-config" && key != "config-file")) {
      cout << indent;
      if (!needs_quotes(key)) {
        cout << key;
      } else {
        detail::print_escaped(out, key);
      }
      if (!holds_alternative<config_value::dictionary>(val)) {
        cout << " = ";
        print_val(val, indent);
        cout << '\n';
      } else if (auto xs = get<config_value::dictionary>(val); xs.empty()) {
        cout << "{}\n";
      } else {
        cout << " {\n";
        print(get<config_value::dictionary>(val), indent + 2);
        cout << indent << "}\n";
      }
    }
  }
}

} // namespace

error actor_system_config::parse(string_list args, std::istream& config) {
  // Contents of the config file override hard-coded defaults.
  if (config.good()) {
    if (auto err = parse_config(config, custom_options_, content))
      return err;
  } else {
    // Not finding an explicitly defined config file is an error.
    if (auto fname = get_if<std::string>(&content, "config-file"))
      return make_error(sec::cannot_open_file, *fname);
  }
  // CLI options override the content of the config file.
  using std::make_move_iterator;
  auto res = custom_options_.parse(content, args);
  if (res.second != args.end()) {
    if (res.first != pec::success && starts_with(*res.second, "-")) {
      return make_error(res.first, *res.second);
    } else {
      args.erase(args.begin(), res.second);
      set_remainder(std::move(args));
    }
  } else {
    cli_helptext_printed = get_or(content, "help", false)
                           || get_or(content, "long-help", false);
    set_remainder(string_list{});
  }
  // Generate help text if needed.
  if (cli_helptext_printed) {
    bool long_help = get_or(content, "long-help", false);
    std::cout << custom_options_.help_text(!long_help) << std::endl;
  }
  // Generate config dump if needed.
  if (!cli_helptext_printed && get_or(content, "dump-config", false)) {
    print(dump_content(), indentation{0});
    std::cout << std::flush;
    cli_helptext_printed = true;
  }
  return none;
}

error actor_system_config::parse(string_list args) {
  if (auto&& [err, path] = extract_config_file_path(args); !err) {
    std::ifstream conf;
    // No error. An empty path simply means no --config-file=ARG was passed.
    if (!path.empty()) {
      conf.open(path);
    } else {
      // Try config_file_path and if that fails try the alternative paths.
      auto try_open = [this, &conf](const auto& what) {
        if (what.empty())
          return false;
        conf.open(what);
        if (conf.is_open()) {
          set("global.config-file", what);
          return true;
        } else {
          return false;
        }
      };
      try_open(config_file_path)
        || std::any_of(config_file_path_alternatives.begin(),
                       config_file_path_alternatives.end(), try_open);
    }
    return parse(std::move(args), conf);
  } else {
    return err;
  }
}

actor_system_config& actor_system_config::add_actor_factory(std::string name,
                                                            actor_factory fun) {
  actor_factories.emplace(std::move(name), std::move(fun));
  return *this;
}

actor_system_config& actor_system_config::set_impl(std::string_view name,
                                                   config_value value) {
  auto opt = custom_options_.qualified_name_lookup(name);
  if (opt == nullptr) {
    std::cerr << "*** failed to set config parameter " << name
              << ": invalid name" << std::endl;
  } else if (auto err = opt->sync(value)) {
    std::cerr << "*** failed to set config parameter " << name << ": "
              << to_string(err) << std::endl;
  } else {
    auto category = opt->category();
    if (category == "global")
      content[opt->long_name()] = std::move(value);
    else
      put(content, name, std::move(value));
  }
  return *this;
}

expected<settings>
actor_system_config::parse_config_file(const char* filename) {
  config_option_set dummy;
  return parse_config_file(filename, dummy);
}

expected<settings>
actor_system_config::parse_config_file(const char* filename,
                                       const config_option_set& opts) {
  std::ifstream f{filename};
  if (!f.is_open())
    return make_error(sec::cannot_open_file, filename);
  return parse_config(f, opts);
}

expected<settings> actor_system_config::parse_config(std::istream& source) {
  config_option_set dummy;
  return parse_config(source, dummy);
}

expected<settings>
actor_system_config::parse_config(std::istream& source,
                                  const config_option_set& opts) {
  settings result;
  if (auto err = parse_config(source, opts, result))
    return err;
  return result;
}

error actor_system_config::parse_config(std::istream& source,
                                        const config_option_set& opts,
                                        settings& result) {
  if (!source)
    return make_error(sec::runtime_error, "source stream invalid");
  detail::config_consumer consumer{opts, result};
  parser_state<config_iter, config_sentinel> res{config_iter{&source}};
  detail::parser::read_config(res, consumer);
  if (res.i != res.e)
    return make_error(res.code, res.line, res.column);
  return none;
}

std::pair<error, std::string>
actor_system_config::extract_config_file_path(string_list& args) {
  auto ptr = custom_options_.qualified_name_lookup("global.config-file");
  CAF_ASSERT(ptr != nullptr);
  string_list::iterator i;
  std::string_view path;
  std::tie(i, path) = find_by_long_name(*ptr, args.begin(), args.end());
  if (i == args.end()) {
    return {none, std::string{}};
  } else if (path.empty()) {
    return {make_error(pec::missing_argument, "no argument to --config-file"),
            std::string{}};
  } else {
    auto path_str = std::string{path.begin(), path.end()};
    args.erase(i);
    config_value val{path_str};
    if (auto err = ptr->sync(val); !err) {
      put(content, "config-file", std::move(val));
      return {none, std::move(path_str)};
    } else {
      return {std::move(err), std::string{}};
    }
  }
}

const settings& content(const actor_system_config& cfg) {
  return cfg.content;
}

} // namespace caf
