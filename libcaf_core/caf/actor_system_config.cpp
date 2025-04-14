// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/actor_system_config.hpp"

#include "caf/config.hpp"
#include "caf/config_option.hpp"
#include "caf/config_option_adder.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/actor_system_config_access.hpp"
#include "caf/detail/assert.hpp"
#include "caf/detail/config_consumer.hpp"
#include "caf/detail/mailbox_factory.hpp"
#include "caf/detail/parser/read_config.hpp"
#include "caf/format_to_error.hpp"
#include "caf/internal/core_config.hpp"
#include "caf/pec.hpp"
#include "caf/sec.hpp"
#include "caf/type_id.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>

namespace caf {

namespace {

using module_factory_list
  = std::vector<actor_system_module* (*) (actor_system&)>;

using actor_factory_dictionary = dictionary<actor_factory>;

using thread_hook_list = std::vector<std::unique_ptr<thread_hook>>;

struct c_args_wrapper {
  int argc = 0;
  char** argv = nullptr;

  static char* copy_str(const std::string& str) {
    if (str.empty()) {
      auto result = new char[1];
      result[0] = '\0';
      return result;
    }
    auto result = new char[str.size() + 1];
    memcpy(result, str.c_str(), str.size() + 1);
    return result;
  }

  void assign(const std::string& program_name) {
    argc = 1;
    argv = new char*[1];
    argv[0] = copy_str(program_name);
  }

  void assign(const std::string& program_name,
              const std::vector<std::string>& args) {
    argc = static_cast<int>(args.size() + 1);
    argv = new char*[argc + 1];
    argv[0] = copy_str(program_name);
    auto index = size_t{1};
    for (auto& arg : args) {
      argv[index++] = copy_str(arg);
    }
  }

  ~c_args_wrapper() {
    for (int i = 0; i < argc; ++i) {
      delete[] argv[i];
    }
    delete[] argv;
  }
};

constexpr const char* default_config_file = "caf-application.conf";

} // namespace

struct actor_system_config::fields {
  std::vector<std::string> paths;
  module_factory_list module_factories;
  actor_factory_dictionary actor_factories;
  thread_hook_list thread_hooks;
  std::unique_ptr<detail::mailbox_factory> mailbox_factory;
  bool helptext_printed = false;
  std::string program_name;
  std::vector<std::string> args_remainder;
  c_args_wrapper c_args_remainder;
  internal::core_config core;
};

using core_t = actor_system_config::core_t;

core_t::logger_t::file_t core_t::logger_t::file_t::path(std::string val) {
  ptr_->path = std::move(val);
  return *this;
};

core_t::logger_t::file_t core_t::logger_t::file_t::format(std::string val) {
  ptr_->format = std::move(val);
  return *this;
}

core_t::logger_t::file_t core_t::logger_t::file_t::verbosity(std::string val) {
  ptr_->verbosity = std::move(val);
  return *this;
}

core_t::logger_t::file_t
core_t::logger_t::file_t::add_excluded_component(std::string val) {
  ptr_->excluded_components.push_back(std::move(val));
  return *this;
}

core_t::logger_t::console_t core_t::logger_t::console_t::colored(bool val) {
  ptr_->colored = val;
  return *this;
}

core_t::logger_t::console_t
core_t::logger_t::console_t::format(std::string val) {
  ptr_->format = std::move(val);
  return *this;
}

core_t::logger_t::console_t
core_t::logger_t::console_t::verbosity(std::string val) {
  ptr_->verbosity = std::move(val);
  return *this;
}

core_t::logger_t::console_t
core_t::logger_t::console_t::add_excluded_component(std::string val) {
  ptr_->excluded_components.push_back(std::move(val));
  return *this;
}

core_t::logger_t::file_t core_t::logger_t::file() {
  return file_t{&ptr_->file};
}

core_t::logger_t core_t::logger_t::add_log_level(std::string name,
                                                 unsigned level) {
  ptr_->log_levels.set(std::move(name), level);
  return *this;
}

core_t::logger_t core_t::logger() {
  return logger_t{&ptr_->logger};
}

// -- constructors, destructors, and assignment operators ----------------------

// in this config class, we have (1) hard-coded defaults that are overridden
// by (2) config file contents that are in turn overridden by (3) CLI arguments

actor_system_config::actor_system_config() {
  fields_ = new fields();
  // Fill our options vector for creating config file and CLI parsers.
  // Note: we set an empty environment variable name for our global flags to
  //       have them only available via CLI.
  opt_group{custom_options_, "global"}
    .add<bool>("help,h?,", "print help text to STDERR and exit")
    .add<bool>("long-help,,",
               "same as --help but list options that are omitted by default")
    .add<bool>("dump-config,,", "print configuration and exit")
    .add<std::string>("config-file", "sets a path to a configuration file");
  opt_group{custom_options_, "caf.scheduler"}
    .add<std::string>("policy", "'stealing' (default) or 'sharing'")
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
  fields_->core.init(custom_options_);
  opt_group{custom_options_, "caf.metrics"} //
    .add<bool>("disable-running-actors",
               "sets whether to collect metrics for running actors per type");
  opt_group{custom_options_, "caf.metrics.filters.actors"}
    .add<std::vector<std::string>>("includes",
                                   "selects actors for run-time metrics")
    .add<std::vector<std::string>>("excludes",
                                   "excludes actors from run-time metrics");
  opt_group{custom_options_, "caf.console"}
    .add<bool>("colored", "forces colored or uncolored output")
    .add<std::string>("stream", "'stdout' (default), 'stderr' or 'none'");
}

actor_system_config::~actor_system_config() {
  delete fields_;
}

// -- properties ---------------------------------------------------------------

core_t actor_system_config::core() {
  return core_t{&fields_->core};
}

settings actor_system_config::dump_content() const {
  settings result = content;
  // Hide options that make no sense in a config file.
  result.erase("dump-config");
  result.erase("config-file");
  auto& caf_group = result["caf"].as_dictionary();
  fields_->core.dump(caf_group);
  // -- scheduler config
  auto& scheduler_group = caf_group["scheduler"].as_dictionary();
  put_missing(scheduler_group, "policy", defaults::scheduler::policy);
  put_missing(scheduler_group, "max-throughput",
              defaults::scheduler::max_throughput);
  // -- work-stealing config
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
  return result;
}

// -- config file parsing ------------------------------------------------------

void actor_system_config::config_file_path(std::string path) {
  config_file_paths(std::vector<std::string>{std::move(path)});
}

void actor_system_config::config_file_paths(std::vector<std::string> paths) {
  fields_->paths = std::move(paths);
}

span<const std::string> actor_system_config::config_file_paths() {
  return fields_->paths;
}

error actor_system_config::parse(int argc, char** argv) {
  std::vector<std::string> args;
  if (argc > 0) {
    fields_->program_name = argv[0];
    if (argc > 1)
      args.assign(argv + 1, argv + argc);
  }
  return parse(std::move(args));
}

error actor_system_config::parse(int argc, char** argv, std::istream& conf) {
  std::vector<std::string> args;
  if (argc > 0) {
    fields_->program_name = argv[0];
    if (argc > 1)
      args.assign(argv + 1, argv + argc);
  }
  return parse(std::move(args), conf);
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

error actor_system_config::parse(std::vector<std::string> args,
                                 std::istream& config) {
  // Contents of the config file override hard-coded defaults.
  if (config.good()) {
    if (auto err = parse_config(config, custom_options_, content))
      return err;
  } else {
    // Not finding an explicitly defined config file is an error.
    if (auto fname = get_if<std::string>(&content, "config-file"))
      return format_to_error(sec::cannot_open_file,
                             "cannot open config file: {}", *fname);
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
    }
    args.erase(args.begin(), res.second);
    set_remainder(std::move(args));
  } else {
    set_remainder({});
    // No value, just having helptext-printed present is the information.
    if (get_or(content, "help", false) || get_or(content, "long-help", false))
      fields_->helptext_printed = true;
  }
  // Generate help text if needed.
  if (helptext_printed()) {
    auto text = custom_options_.help_text(!get_or(content, "long-help", false));
    puts(text.c_str());
  }
  // Check for invalid options.
  if (auto err = fields_->core.validate()) {
    return err;
  }
  // Generate config dump if needed.
  if (!helptext_printed() && get_or(content, "dump-config", false)) {
    print_content();
    fields_->helptext_printed = true;
  }
  return {};
}

error actor_system_config::parse(std::vector<std::string> args) {
  if (auto&& [err, path] = extract_config_file_path(args); !err) {
    std::ifstream conf;
    if (!path.empty()) {
      conf.open(path);
    } else {
      // Try the user-defined config file paths or fall back to the default.
      auto try_open = [this, &conf](auto&& what) {
        conf.open(what);
        if (conf.is_open()) {
          set("global.config-file", what);
          return true;
        } else {
          return false;
        }
      };
      auto paths = config_file_paths();
      if (paths.empty())
        try_open(default_config_file);
      else
        std::ignore = std::any_of(paths.begin(), paths.end(), try_open);
      // Note: not finding any file is not an error. It simply means that we
      //       use the hard-coded defaults.
    }
    return parse(std::move(args), conf);
  } else {
    return err;
  }
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
    return format_to_error(sec::cannot_open_file, "cannot open config file: {}",
                           filename);
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
    return format_to_error(
      res.code, "failed to parse config: invalid syntax in line {} column {}",
      res.line, res.column);
  return none;
}

std::pair<error, std::string>
actor_system_config::extract_config_file_path(std::vector<std::string>& args) {
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

const settings& content(const actor_system_config& cfg) {
  return cfg.content;
}

void actor_system_config::print_content() const {
  config_printer printer;
  printer(dump_content());
  std::cout << std::endl;
}

// -- module factories ---------------------------------------------------------

void actor_system_config::add_module_factory(module_factory_fn ptr) {
  fields_->module_factories.emplace_back(ptr);
}

span<actor_system_config::module_factory_fn>
actor_system_config::module_factories() {
  return fields_->module_factories;
}

// -- actor factories ----------------------------------------------------------

actor_system_config& actor_system_config::add_actor_factory(std::string name,
                                                            actor_factory fun) {
  fields_->actor_factories.insert_or_assign(std::move(name), std::move(fun));
  return *this;
}

actor_factory* actor_system_config::get_actor_factory(std::string_view name) {
  auto i = fields_->actor_factories.find(name);
  if (i == fields_->actor_factories.end())
    return nullptr;
  return &i->second;
}

// -- thread hooks -------------------------------------------------------------

void actor_system_config::add_thread_hook(std::unique_ptr<thread_hook> ptr) {
  fields_->thread_hooks.emplace_back(std::move(ptr));
}

span<std::unique_ptr<thread_hook>> actor_system_config::thread_hooks() {
  return fields_->thread_hooks;
}

// -- mailbox factory ----------------------------------------------------------

void actor_system_config::mailbox_factory(
  std::unique_ptr<detail::mailbox_factory> factory) {
  fields_->mailbox_factory = std::move(factory);
}

detail::mailbox_factory* actor_system_config::mailbox_factory() {
  return fields_->mailbox_factory.get();
}

// -- internal bookkeeping -----------------------------------------------------

bool actor_system_config::helptext_printed() const noexcept {
  return fields_->helptext_printed;
}

const std::string& actor_system_config::program_name() const noexcept {
  return fields_->program_name;
}

void actor_system_config::set_remainder(std::vector<std::string> args) {
  fields_->c_args_remainder.assign(program_name(), args);
  fields_->args_remainder = std::move(args);
}

span<const std::string> actor_system_config::remainder() const noexcept {
  return fields_->args_remainder;
}

std::pair<int, char**> actor_system_config::c_args_remainder() const noexcept {
  return std::make_pair(fields_->c_args_remainder.argc,
                        fields_->c_args_remainder.argv);
}

} // namespace caf

// -- member functions from actor_system_config_access -------------------------

namespace caf::detail {

internal::core_config& actor_system_config_access::core() {
  return cfg_->fields_->core;
}

const internal::core_config& const_actor_system_config_access::core() const {
  return cfg_->fields_->core;
}

} // namespace caf::detail
