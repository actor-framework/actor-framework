// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/actor_system_config.hpp"

#include "caf/config.hpp"
#include "caf/config_option.hpp"
#include "caf/config_option_adder.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/config_consumer.hpp"
#include "caf/detail/parser/read_config.hpp"
#include "caf/detail/parser/read_string.hpp"
#include "caf/logger.hpp"
#include "caf/message_builder.hpp"
#include "caf/pec.hpp"
#include "caf/sec.hpp"
#include "caf/type_id.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <thread>

namespace caf {

namespace {

constexpr std::string_view default_program_name = "unknown-caf-app";

constexpr std::string_view default_config_file = "caf-application.conf";

} // namespace

// -- constructors, destructors, and assignment operators ----------------------

actor_system_config::~actor_system_config() {
  // nop
}

// in this config class, we have (1) hard-coded defaults that are overridden
// by (2) config file contents that are in turn overridden by (3) CLI arguments

actor_system_config::actor_system_config()
  : program_name(default_program_name), config_file_path(default_config_file) {
  // Fill our options vector for creating config file and CLI parsers.
  using std::string;
  using string_list = std::vector<string>;
  // Note: set an empty environment variable name for our global flags to have
  //       them only available via CLI.
  opt_group{custom_options_, "global"}
    .add<bool>("help,h?,", "print help text to STDERR and exit")
    .add<bool>("long-help,,",
               "same as --help but list options that are omitted by default")
    .add<bool>("dump-config,,", "print configuration and exit")
    .add<string>("config-file", "sets a path to a configuration file");
  opt_group{custom_options_, "caf.scheduler"}
    .add<string>("policy", "'stealing' (default) or 'sharing'")
    .add<size_t>("max-threads", "maximum number of worker threads")
    .add<size_t>("max-throughput",
                 "nr. of messages actors can consume per run");
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

// -- properties ---------------------------------------------------------------

settings actor_system_config::dump_content() const {
  settings result = content;
  // Hide options that make no sense in a config file.
  result.erase("dump-config");
  result.erase("config-file");
  auto& caf_group = result["caf"].as_dictionary();
  // -- scheduler parameters
  auto& scheduler_group = caf_group["scheduler"].as_dictionary();
  put_missing(scheduler_group, "policy", defaults::scheduler::policy);
  put_missing(scheduler_group, "max-throughput",
              defaults::scheduler::max_throughput);
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
  auto& file_group = logger_group["file"].as_dictionary();
  put_missing(file_group, "path", defaults::logger::file::path);
  put_missing(file_group, "format", defaults::logger::file::format);
  put_missing(file_group, "excluded-components", std::vector<std::string>{});
  auto& console_group = logger_group["console"].as_dictionary();
  put_missing(console_group, "colored", defaults::logger::console::colored);
  put_missing(console_group, "format", defaults::logger::console::format);
  put_missing(console_group, "excluded-components", std::vector<std::string>{});
  return result;
}

// -- modifiers ----------------------------------------------------------------

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
  size_t size = 0;
};

indentation operator+(indentation x, size_t y) noexcept {
  return {x.size + y};
}

std::ostream& operator<<(std::ostream& out, indentation indent) {
  for (size_t i = 0; i < indent.size; ++i)
    out.put(' ');
  return out;
}

struct cout_pseudo_buf {
  using value_type = char;

  void push_back(char ch) {
    std::cout.put(ch);
  }

  unit_t end() {
    return unit;
  }

  template <class Iterator, class Sentinel>
  void insert(unit_t, Iterator first, Sentinel last) {
    while (first != last)
      std::cout.put(*first++);
  }
};

class config_printer {
public:
  config_printer() = default;

  explicit config_printer(indentation indent, bool nested = false)
    : indent_(indent), nested_(nested) {
    // nop
  }

  void operator()(const config_value& val) {
    std::visit(*this, val.get_data());
  }

  void operator()(none_t) {
    detail::print(out_, none);
  }

  template <class T>
  std::enable_if_t<std::is_arithmetic_v<T>> operator()(T val) {
    detail::print(out_, val);
  }

  void operator()(timespan val) {
    detail::print(out_, val);
  }

  void operator()(const uri& val) {
    std::cout << '<' << val.str() << '>';
  }

  void operator()(const std::string& str) {
    detail::print_escaped(out_, str);
  }

  void operator()(const config_value::list& xs) {
    if (xs.empty()) {
      std::cout << "[]";
      return;
    }
    std::cout << "[\n";
    auto nestd_indent = indent_ + 2;
    for (auto& x : xs) {
      std::cout << nestd_indent;
      std::visit(config_printer{nestd_indent, true}, x.get_data());
      std::cout << ",\n";
    }
    std::cout << indent_ << "]";
  }

  void operator()(const config_value::dictionary& dict) {
    if (dict.empty()) {
      std::cout << "{}";
      return;
    }
    if (!nested_) {
      auto pos = dict.begin();
      for (;;) {
        print_kvp(*pos++);
        if (pos == dict.end())
          return;
        std::cout << '\n';
      }
    }
    std::cout << "{\n";
    auto nestd_indent = indent_ + 2;
    auto pos = dict.begin();
    for (;;) {
      config_printer{nestd_indent, true}.print_kvp(*pos++);
      if (pos == dict.end()) {
        std::cout << '\n';
        std::cout << indent_ << "}";
        return;
      }
      std::cout << '\n';
    }
  }

private:
  void print_kvp(const config_value::dictionary::value_type& kvp) {
    const auto& [key, val] = kvp;
    if (auto* submap = get_if<config_value::dictionary>(&val)) {
      std::cout << indent_;
      print_key(key);
      std::cout << " {\n";
      auto sub_printer = config_printer{indent_ + 2};
      sub_printer(*submap);
      std::cout << "\n" << indent_ << "}";
    } else {
      std::cout << indent_;
      print_key(key);
      std::cout << " = ";
      std::visit(*this, val.get_data());
    }
  }

  void print_key(const std::string& key) {
    if (key.find('.') == std::string::npos)
      std::cout << key;
    else
      detail::print_escaped(out_, key);
  }

  indentation indent_;
  bool nested_ = false;
  cout_pseudo_buf out_;
};

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
  // Environment variables override the content of the config file.
  for (auto& opt : custom_options_) {
    const auto* env_var_name = opt.env_var_name_cstr();
    CAF_ASSERT(env_var_name != nullptr);
    if (env_var_name[0] == '\0') {
      // Passing an empty string to `getenv` will set `errno`, so we simply skip
      // empty environment variable names to avoid this.
      continue;
    }
    if (auto* env_var = getenv(env_var_name)) {
      config_value value{env_var};
      if (auto err = opt.sync(value); !err) {
        if (opt.category() == "global")
          put(content, opt.long_name(), std::move(value));
        else
          put(content, opt.full_name(), std::move(value));
      } else {
        return err;
      }
    }
  }
  // CLI options override everything.
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
    print_content();
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
    if (opt->category() == "global")
      put(content, opt->long_name(), std::move(value));
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
  auto result = std::string{};
  // Look for the environment variable first.
  const auto* env_var_name = ptr->env_var_name_cstr();
  CAF_ASSERT(env_var_name != nullptr);
  if (env_var_name[0] != '\0') {
    if (auto* path = getenv(env_var_name)) {
      result = path;
      put(content, "config-file", result);
    }
  }
  // Look for the command line argument second (overrides the env var).
  auto [first, last, path] = ptr->find_by_long_name(args.begin(), args.end());
  if (first == args.end()) {
    return {none, result};
  }
  if (path.empty()) {
    return {make_error(pec::missing_argument, "no argument to --config-file"),
            std::string{}};
  }
  auto path_str = std::string{path};
  args.erase(first, last);
  config_value val{path_str};
  if (auto err = ptr->sync(val)) {
    return {std::move(err), std::string{}};
  }
  put(content, "config-file", std::move(val));
  return {none, std::move(path_str)};
}

detail::mailbox_factory* actor_system_config::mailbox_factory() {
  return nullptr;
}

const settings& content(const actor_system_config& cfg) {
  return cfg.content;
}

void actor_system_config::print_content() const {
  config_printer printer;
  printer(dump_content());
  std::cout << std::endl;
}

// -- factories ----------------------------------------------------------------

intrusive_ptr<logger>
actor_system_config::make_logger(actor_system& sys) const {
  if (logger_factory_)
    return logger_factory_(sys);
  return logger::make(sys);
}

} // namespace caf
