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

#include "caf/actor_system_config.hpp"

#include <fstream>
#include <limits>
#include <sstream>
#include <thread>

#include "caf/config.hpp"
#include "caf/config_option.hpp"
#include "caf/config_option_adder.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/gcd.hpp"
#include "caf/detail/ini_consumer.hpp"
#include "caf/detail/parser/read_ini.hpp"
#include "caf/detail/parser/read_string.hpp"
#include "caf/init_global_meta_objects.hpp"
#include "caf/message_builder.hpp"
#include "caf/type_id.hpp"

namespace caf {

namespace {

constexpr const char* default_config_file = "caf-application.ini";

} // namespace

actor_system_config::~actor_system_config() {
  // nop
}

// in this config class, we have (1) hard-coded defaults that are overridden
// by (2) INI-file contents that are in turn overridden by (3) CLI arguments

actor_system_config::actor_system_config()
  : cli_helptext_printed(false),
    slave_mode(false),
    config_file_path(default_config_file),
    slave_mode_fun(nullptr) {
  init_global_meta_objects<builtin_type_ids>();
  // (1) hard-coded defaults
  stream_desired_batch_complexity = defaults::stream::desired_batch_complexity;
  stream_max_batch_delay = defaults::stream::max_batch_delay;
  stream_credit_round_interval = defaults::stream::credit_round_interval;
  // fill our options vector for creating INI and CLI parsers
  using std::string;
  opt_group{custom_options_, "global"}
    .add<bool>("help,h?", "print help text to STDERR and exit")
    .add<bool>("long-help", "print long help text to STDERR and exit")
    .add<bool>("dump-config", "print configuration to STDERR and exit")
    .add<string>(config_file_path, "config-file",
                 "set config file (default: caf-application.ini)");
  opt_group{custom_options_, "stream"}
    .add<timespan>(stream_desired_batch_complexity, "desired-batch-complexity",
                   "processing time per batch")
    .add<timespan>(stream_max_batch_delay, "max-batch-delay",
                   "maximum delay for partial batches")
    .add<timespan>(stream_credit_round_interval, "credit-round-interval",
                   "time between emitting credit")
    .add<std::string>("credit-policy",
                      "selects an algorithm for credit computation");
  opt_group{custom_options_, "scheduler"}
    .add<std::string>("policy", "'stealing' (default) or 'sharing'")
    .add<size_t>("max-threads", "maximum number of worker threads")
    .add<size_t>("max-throughput", "nr. of messages actors can consume per run")
    .add<bool>("enable-profiling", "enables profiler output")
    .add<timespan>("profiling-resolution", "data collection rate")
    .add<string>("profiling-output-file", "output file for the profiler");
  opt_group(custom_options_, "work-stealing")
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
  opt_group{custom_options_, "logger"}
    .add<std::string>("verbosity", "default verbosity for file and console")
    .add<string>("file-name", "filesystem path of the log file")
    .add<string>("file-format", "line format for individual log file entries")
    .add<std::string>("file-verbosity", "file output verbosity")
    .add<std::string>("console",
                      "std::clog output: none, colored, or uncolored")
    .add<string>("console-format", "line format for printed log entries")
    .add<std::string>("console-verbosity", "console output verbosity")
    .add<std::vector<std::string>>("component-blacklist",
                                   "excluded components for logging")
    .add<bool>("inline-output", "disable logger thread (for testing only!)");
  opt_group{custom_options_, "middleman"}
    .add<std::string>("network-backend",
                      "either 'default' or 'asio' (if available)")
    .add<std::vector<string>>("app-identifiers",
                              "valid application identifiers of this node")
    .add<string>("app-identifier", "DEPRECATED: use app-identifiers instead")
    .add<bool>("enable-automatic-connections",
               "enables automatic connection management")
    .add<size_t>("max-consecutive-reads",
                 "max. number of consecutive reads per broker")
    .add<timespan>("heartbeat-interval", "interval of heartbeat messages")
    .add<bool>("attach-utility-actors",
               "schedule utility actors instead of dedicating threads")
    .add<bool>("manual-multiplexing",
               "disables background activity of the multiplexer")
    .add<size_t>("workers", "number of deserialization workers");
  opt_group(custom_options_, "openssl")
    .add<string>(openssl_certificate, "certificate",
                 "path to the PEM-formatted certificate file")
    .add<string>(openssl_key, "key",
                 "path to the private key file for this node")
    .add<string>(openssl_passphrase, "passphrase",
                 "passphrase to decrypt the private key")
    .add<string>(openssl_capath, "capath",
                 "path to an OpenSSL-style directory of trusted certificates")
    .add<string>(openssl_cafile, "cafile",
                 "path to a file of concatenated PEM-formatted certificates");
  // add renderers for default error categories
  add_error_category(error_category<sec>::value, render_sec);
  add_error_category(error_category<pec>::value, render_pec);
  add_error_category(error_category<exit_reason>::value, render_exit_reason);
}

settings actor_system_config::dump_content() const {
  settings result = content;
  // -- streaming parameters
  auto& stream_group = result["stream"].as_dictionary();
  put_missing(stream_group, "desired-batch-complexity",
              defaults::stream::desired_batch_complexity);
  put_missing(stream_group, "max-batch-delay",
              defaults::stream::max_batch_delay);
  put_missing(stream_group, "credit-round-interval",
              defaults::stream::credit_round_interval);
  put_missing(stream_group, "credit-policy", defaults::stream::credit_policy);
  put_missing(stream_group, "size-policy.buffer-capacity",
              defaults::stream::size_policy::buffer_capacity);
  put_missing(stream_group, "size-policy.bytes-per-batch",
              defaults::stream::size_policy::bytes_per_batch);
  // -- scheduler parameters
  auto& scheduler_group = result["scheduler"].as_dictionary();
  put_missing(scheduler_group, "policy", defaults::scheduler::policy);
  put_missing(scheduler_group, "max-threads", defaults::scheduler::max_threads);
  put_missing(scheduler_group, "max-throughput",
              defaults::scheduler::max_throughput);
  put_missing(scheduler_group, "enable-profiling", false);
  put_missing(scheduler_group, "profiling-resolution",
              defaults::scheduler::profiling_resolution);
  put_missing(scheduler_group, "profiling-output-file", std::string{});
  // -- work-stealing parameters
  auto& work_stealing_group = result["work-stealing"].as_dictionary();
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
  auto& logger_group = result["logger"].as_dictionary();
  put_missing(logger_group, "file-name", defaults::logger::file_name);
  put_missing(logger_group, "file-format", defaults::logger::file_format);
  put_missing(logger_group, "file-verbosity", defaults::logger::file_verbosity);
  put_missing(logger_group, "console", defaults::logger::console);
  put_missing(logger_group, "console-format", defaults::logger::console_format);
  put_missing(logger_group, "console-verbosity",
              defaults::logger::console_verbosity);
  put_missing(logger_group, "component-blacklist", std::vector<std::string>{});
  put_missing(logger_group, "inline-output", false);
  // -- middleman parameters
  auto& middleman_group = result["middleman"].as_dictionary();
  put_missing(middleman_group, "app-identifiers",
              defaults::middleman::app_identifiers);
  put_missing(middleman_group, "enable-automatic-connections", false);
  put_missing(middleman_group, "max-consecutive-reads",
              defaults::middleman::max_consecutive_reads);
  put_missing(middleman_group, "heartbeat-interval",
              defaults::middleman::heartbeat_interval);
  put_missing(middleman_group, "workers", defaults::middleman::workers);
  // -- openssl parameters
  auto& openssl_group = result["openssl"].as_dictionary();
  put_missing(openssl_group, "certificate", std::string{});
  put_missing(openssl_group, "key", std::string{});
  put_missing(openssl_group, "passphrase", std::string{});
  put_missing(openssl_group, "capath", std::string{});
  put_missing(openssl_group, "cafile", std::string{});
  return result;
}

error actor_system_config::parse(int argc, char** argv,
                                 const char* ini_file_cstr) {
  string_list args;
  if (argc > 1)
    args.assign(argv + 1, argv + argc);
  return parse(std::move(args), ini_file_cstr);
}

error actor_system_config::parse(int argc, char** argv, std::istream& ini) {
  string_list args;
  if (argc > 1)
    args.assign(argv + 1, argv + argc);
  return parse(std::move(args), ini);
}

namespace {

struct ini_iter {
  std::istream* ini;
  char ch;

  explicit ini_iter(std::istream* istr) : ini(istr) {
    ini->get(ch);
  }

  ini_iter() : ini(nullptr), ch('\0') {
    // nop
  }

  ini_iter(const ini_iter&) = default;

  ini_iter& operator=(const ini_iter&) = default;

  inline char operator*() const {
    return ch;
  }

  inline ini_iter& operator++() {
    ini->get(ch);
    return *this;
  }
};

struct ini_sentinel {};

bool operator!=(ini_iter iter, ini_sentinel) {
  return !iter.ini->fail();
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

void print(const config_value::dictionary& xs, indentation indent) {
  using std::cout;
  for (const auto& kvp : xs) {
    if (kvp.first == "dump-config")
      continue;
    if (auto submap = get_if<config_value::dictionary>(&kvp.second)) {
      cout << indent << kvp.first << " {\n";
      print(*submap, indent + 2);
      cout << indent << "}\n";
    } else if (auto lst = get_if<config_value::list>(&kvp.second)) {
      if (lst->empty()) {
        cout << indent << kvp.first << " = []\n";
      } else {
        cout << indent << kvp.first << " = [\n";
        auto list_indent = indent + 2;
        for (auto& x : *lst)
          cout << list_indent << to_string(x) << ",\n";
        cout << indent << "]\n";
      }
    } else {
      cout << indent << kvp.first << " = " << to_string(kvp.second) << '\n';
    }
  }
}

} // namespace

error actor_system_config::parse(string_list args, std::istream& ini) {
  // Content of the INI file overrides hard-coded defaults.
  if (ini.good()) {
    if (auto err = parse_config(ini, custom_options_, content))
      return err;
  } else {
    // Not finding an explicitly defined config file is an error.
    if (auto fname = get_if<std::string>(&content, "config-file"))
      return make_error(sec::cannot_open_file, *fname);
  }
  // CLI options override the content of the INI file.
  using std::make_move_iterator;
  auto res = custom_options_.parse(content, args);
  if (res.second != args.end()) {
    if (res.first != pec::success && starts_with(*res.second, "-"))
      return make_error(res.first, *res.second);
    auto first = args.begin();
    first += std::distance(args.cbegin(), res.second);
    remainder.insert(remainder.end(), make_move_iterator(first),
                     make_move_iterator(args.end()));
  } else {
    cli_helptext_printed = get_or(content, "help", false)
                           || get_or(content, "long-help", false);
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
  return adjust_content();
}

error actor_system_config::parse(string_list args, const char* ini_file_cstr) {
  // Override default config file name if set by user.
  if (ini_file_cstr != nullptr)
    config_file_path = ini_file_cstr;
  // CLI arguments always win.
  if (auto err = extract_config_file_path(args))
    return err;
  std::ifstream ini{config_file_path};
  return parse(std::move(args), ini);
}

actor_system_config& actor_system_config::add_actor_factory(std::string name,
                                                            actor_factory fun) {
  actor_factories.emplace(std::move(name), std::move(fun));
  return *this;
}

actor_system_config& actor_system_config::add_error_category(uint8_t x,
                                                             error_renderer y) {
  error_renderers[x] = y;
  return *this;
}

actor_system_config&
actor_system_config::set_impl(string_view name, config_value value) {
  if (name == "middleman.app-identifier") {
    // TODO: Print a warning with 0.18 and remove this code with 0.19.
    value.convert_to_list();
    return set_impl("middleman.app-identifiers", std::move(value));
  }
  auto opt = custom_options_.qualified_name_lookup(name);
  if (opt == nullptr) {
    std::cerr << "*** failed to set config parameter " << name
              << ": invalid name" << std::endl;
  } else if (auto err = opt->check(value)) {
    std::cerr << "*** failed to set config parameter " << name << ": "
              << to_string(err) << std::endl;
  } else {
    opt->store(value);
    auto category = opt->category();
    auto& dict = category == "global" ? content
                                      : content[category].as_dictionary();
    dict[opt->long_name()] = std::move(value);
  }
  return *this;
}

timespan actor_system_config::stream_tick_duration() const noexcept {
  auto ns_count = caf::detail::gcd(stream_credit_round_interval.count(),
                                   stream_max_batch_delay.count());
  return timespan{ns_count};
}
std::string actor_system_config::render(const error& x) {
  if (!x)
    return "none";
  switch (x.category()) {
    case error_category<sec>::value:
      return render_sec(x.code(), x.context());
    case error_category<exit_reason>::value:
      return render_exit_reason(x.code(), x.context());
    case error_category<pec>::value:
      return render_pec(x.code(), x.context());
    default:
      return deep_to_string(meta::type_name("error"), x.code(), x.category(),
                            meta::omittable_if_empty(), x.context());
  }
}

std::string actor_system_config::render_sec(uint8_t x, const message& xs) {
  auto tmp = static_cast<sec>(x);
  return deep_to_string(meta::type_name("system_error"), tmp,
                        meta::omittable_if_empty(), xs);
}

std::string actor_system_config::render_exit_reason(uint8_t x,
                                                    const message& xs) {
  auto tmp = static_cast<exit_reason>(x);
  return deep_to_string(meta::type_name("exit_reason"), tmp,
                        meta::omittable_if_empty(), xs);
}

std::string actor_system_config::render_pec(uint8_t x, const message& xs) {
  auto tmp = static_cast<pec>(x);
  return deep_to_string(meta::type_name("parser_error"), tmp,
                        meta::omittable_if_empty(), xs);
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
  detail::ini_consumer consumer{opts, result};
  parser_state<ini_iter, ini_sentinel> res{ini_iter{&source}};
  detail::parser::read_ini(res, consumer);
  if (res.i != res.e)
    return make_error(res.code, res.line, res.column);
  return none;
}

error actor_system_config::extract_config_file_path(string_list& args) {
  auto ptr = custom_options_.qualified_name_lookup("global.config-file");
  CAF_ASSERT(ptr != nullptr);
  string_list::iterator i;
  string_view path;
  std::tie(i, path) = find_by_long_name(*ptr, args.begin(), args.end());
  if (i == args.end())
    return none;
  if (path.empty()) {
    args.erase(i);
    return make_error(pec::missing_argument, std::string{*i});
  }
  auto evalue = ptr->parse(path);
  if (!evalue)
    return std::move(evalue.error());
  put(content, "config-file", *evalue);
  ptr->store(*evalue);
  return none;
}

error actor_system_config::adjust_content() {
  // TODO: Print a warning to STDERR if 'app-identifier' is present with 0.18
  //       and remove this code with 0.19.
  auto i = content.find("middleman");
  if (i != content.end()) {
    if (auto mm = get_if<settings>(&i->second)) {
      auto j = mm->find("app-identifier");
      if (j != mm->end()) {
        if (!mm->contains("app-identifiers")) {
          j->second.convert_to_list();
          mm->emplace("app-identifiers", std::move(j->second));
        }
        mm->container().erase(j);
      }
    }
  }
  return none;
}

const settings& content(const actor_system_config& cfg) {
  return cfg.content;
}

} // namespace caf
