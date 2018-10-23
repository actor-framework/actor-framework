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

#include <limits>
#include <thread>
#include <fstream>
#include <sstream>

#include "caf/config.hpp"
#include "caf/config_option_adder.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/gcd.hpp"
#include "caf/detail/ini_consumer.hpp"
#include "caf/detail/parser/read_ini.hpp"
#include "caf/detail/parser/read_string.hpp"
#include "caf/message_builder.hpp"

CAF_PUSH_DEPRECATED_WARNING

namespace caf {

actor_system_config::~actor_system_config() {
  // nop
}

// in this config class, we have (1) hard-coded defaults that are overridden
// by (2) INI-file contents that are in turn overridden by (3) CLI arguments

actor_system_config::actor_system_config()
    : cli_helptext_printed(false),
      slave_mode(false),
      config_file_path("caf-application.ini"),
      slave_mode_fun(nullptr) {
  // add `vector<T>` and `stream<T>` for each statically known type
  add_message_type_impl<stream<actor>>("stream<@actor>");
  add_message_type_impl<stream<actor_addr>>("stream<@addr>");
  add_message_type_impl<stream<atom_value>>("stream<@atom>");
  add_message_type_impl<stream<message>>("stream<@message>");
  add_message_type_impl<std::vector<actor>>("std::vector<@actor>");
  add_message_type_impl<std::vector<actor_addr>>("std::vector<@addr>");
  add_message_type_impl<std::vector<atom_value>>("std::vector<@atom>");
  add_message_type_impl<std::vector<message>>("std::vector<@message>");
  // (1) hard-coded defaults
  stream_desired_batch_complexity = defaults::stream::desired_batch_complexity;
  stream_max_batch_delay = defaults::stream::max_batch_delay;
  stream_credit_round_interval = defaults::stream::credit_round_interval;
  namespace sr = defaults::scheduler;
  auto to_ms = [](timespan x) {
    return static_cast<size_t>(x.count() / 1000000);
  };
  scheduler_policy = sr::policy;
  scheduler_max_threads = sr::max_threads;
  scheduler_max_throughput = sr::max_throughput;
  scheduler_enable_profiling = false;
  scheduler_profiling_ms_resolution = to_ms(sr::profiling_resolution);
  namespace ws = defaults::work_stealing;
  auto to_us = [](timespan x) {
    return static_cast<size_t>(x.count() / 1000);
  };
  work_stealing_aggressive_poll_attempts = ws::aggressive_poll_attempts;
  work_stealing_aggressive_steal_interval = ws::aggressive_steal_interval;
  work_stealing_moderate_poll_attempts = ws::moderate_poll_attempts;
  work_stealing_moderate_steal_interval = ws::moderate_steal_interval;
  work_stealing_moderate_sleep_duration_us = to_us(ws::moderate_sleep_duration);
  work_stealing_relaxed_steal_interval = ws::relaxed_steal_interval;
  work_stealing_relaxed_sleep_duration_us = to_us(ws::relaxed_sleep_duration);
  namespace lg = defaults::logger;
  logger_file_name = lg::file_name;
  logger_file_format = lg::file_format;
  logger_console = lg::console;
  logger_console_format = lg::console_format;
  logger_inline_output = false;
  logger_verbosity = lg::file_verbosity;
  namespace mm = defaults::middleman;
  middleman_network_backend = mm::network_backend;
  middleman_enable_automatic_connections = false;
  middleman_max_consecutive_reads = mm::max_consecutive_reads;
  middleman_heartbeat_interval = mm::heartbeat_interval;
  middleman_detach_utility_actors = true;
  middleman_detach_multiplexer = true;
  middleman_cached_udp_buffers = mm::cached_udp_buffers;
  middleman_max_pending_msgs = mm::max_pending_msgs;
  // fill our options vector for creating INI and CLI parsers
  opt_group{custom_options_, "global"}
  .add<bool>("help,h?", "print help and exit")
  .add<bool>("long-help", "print all help options and exit")
  .add<bool>("dump-config", "print configuration in INI format and exit");
  opt_group{custom_options_, "stream"}
  .add(stream_desired_batch_complexity, "desired-batch-complexity",
       "sets the desired timespan for a single batch")
  .add(stream_max_batch_delay, "max-batch-delay",
       "sets the maximum delay for sending underfull batches")
  .add(stream_credit_round_interval, "credit-round-interval",
       "sets the length of credit intervals");
  opt_group{custom_options_, "scheduler"}
  .add(scheduler_policy, "policy",
       "sets the scheduling policy to either 'stealing' (default) or 'sharing'")
  .add(scheduler_max_threads, "max-threads",
       "sets a fixed number of worker threads for the scheduler")
  .add(scheduler_max_throughput, "max-throughput",
       "sets the maximum number of messages an actor consumes before yielding")
  .add(scheduler_enable_profiling, "enable-profiling",
       "enables or disables profiler output")
  .add_ms(scheduler_profiling_ms_resolution, "profiling-ms-resolution",
       "deprecated (use profiling-resolution instead)")
  .add_ms(scheduler_profiling_ms_resolution, "profiling-resolution",
          "sets the rate in ms in which the profiler collects data")
  .add(scheduler_profiling_output_file, "profiling-output-file",
       "sets the output file for the profiler");
  opt_group(custom_options_, "work-stealing")
  .add(work_stealing_aggressive_poll_attempts, "aggressive-poll-attempts",
       "number of zero-sleep-interval polling attempts")
  .add(work_stealing_aggressive_steal_interval, "aggressive-steal-interval",
       "frequency of steal attempts during aggressive polling")
  .add(work_stealing_moderate_poll_attempts, "moderate-poll-attempts",
       "number of moderately aggressive polling attempts")
  .add(work_stealing_moderate_steal_interval, "moderate-steal-interval",
       "frequency of steal attempts during moderate polling")
  .add_us(work_stealing_moderate_sleep_duration_us, "moderate-sleep-duration",
          "sleep duration between poll attempts during moderate polling")
  .add(work_stealing_relaxed_steal_interval, "relaxed-steal-interval",
       "frequency of steal attempts during relaxed polling")
  .add_us(work_stealing_relaxed_sleep_duration_us, "relaxed-sleep-duration",
          "sleep duration between poll attempts during relaxed polling");
  opt_group{custom_options_, "logger"}
  .add(logger_file_name, "file-name",
       "sets the filesystem path of the log file")
  .add(logger_file_format, "file-format",
       "sets the line format for individual log file entires")
  .add<atom_value>("file-verbosity",
       "sets the file output verbosity (quiet|error|warning|info|debug|trace)")
  .add(logger_console, "console",
       "sets the type of output to std::clog (none|colored|uncolored)")
  .add(logger_console_format, "console-format",
       "sets the line format for printing individual log entires")
  .add<atom_value>("console-verbosity",
       "sets the console output verbosity "
       "(quiet|error|warning|info|debug|trace)")
  .add(logger_component_filter, "component-filter",
       "exclude all listed components from logging")
  .add(logger_verbosity, "verbosity",
       "set file and console verbosity (deprecated)")
  .add(logger_inline_output, "inline-output",
       "sets whether a separate thread is used for I/O");
  opt_group{custom_options_, "middleman"}
  .add(middleman_network_backend, "network-backend",
       "sets the network backend to either 'default' or 'asio' (if available)")
  .add(middleman_app_identifier, "app-identifier",
       "sets the application identifier of this node")
  .add(middleman_enable_automatic_connections, "enable-automatic-connections",
       "enables automatic connection management")
  .add(middleman_max_consecutive_reads, "max-consecutive-reads",
       "sets the maximum number of consecutive I/O reads per broker")
  .add(middleman_heartbeat_interval, "heartbeat-interval",
       "sets the interval (ms) of heartbeat, 0 (default) means disabling it")
  .add(middleman_detach_utility_actors, "detach-utility-actors",
       "deprecated, see attach-utility-actors instead")
  .add_neg(middleman_detach_utility_actors, "attach-utility-actors",
           "schedule utility actors instead of dedicating individual threads")
  .add(middleman_detach_multiplexer, "detach-multiplexer",
       "deprecated, see manual-multiplexing instead")
  .add_neg(middleman_detach_multiplexer, "manual-multiplexing",
           "disables background activity of the multiplexer")
  .add(middleman_cached_udp_buffers, "cached-udp-buffers",
       "sets the maximum for cached UDP send buffers (default: 10)")
  .add(middleman_max_pending_msgs, "max-pending-messages",
       "sets the maximum for reordering of UDP receive buffers (default: 10)")
  .add<bool>("disable-tcp", "disables communication via TCP")
  .add<bool>("enable-udp", "enable communication via UDP");
  opt_group(custom_options_, "opencl")
  .add(opencl_device_ids, "device-ids",
       "restricts which OpenCL devices are accessed by CAF");
  opt_group(custom_options_, "openssl")
  .add(openssl_certificate, "certificate",
       "sets the path to the file containining the certificate for this node PEM format")
  .add(openssl_key, "key",
       "sets the path to the file containting the private key for this node")
  .add(openssl_passphrase, "passphrase",
       "sets the passphrase to decrypt the private key, if needed")
  .add(openssl_capath, "capath",
       "sets the path to an OpenSSL-style directory of trusted certificates")
  .add(openssl_cafile, "cafile",
       "sets the path to a file containing trusted certificates concatenated together in PEM format");
  // add renderers for default error categories
  error_renderers.emplace(atom("system"), render_sec);
  error_renderers.emplace(atom("exit"), render_exit_reason);
}

std::string
actor_system_config::make_help_text(const std::vector<message::cli_arg>& xs) {
  auto is_no_caf_option = [](const message::cli_arg& arg) {
    return arg.name.compare(0, 4, "caf#") != 0;
  };
  auto op = [](size_t tmp, const message::cli_arg& arg) {
    return std::max(tmp, arg.helptext.size());
  };
  // maximum string lenght of all options
  auto name_width = std::accumulate(xs.begin(), xs.end(), size_t{0}, op);
  // iterators to the vector with respect to partition point
  auto first = xs.begin();
  auto last = xs.end();
  auto sep = std::find_if(first, last, is_no_caf_option);
  // output stream
  std::ostringstream oss;
  oss << std::left;
  oss << "CAF Options:" << std::endl;
  for (auto i = first; i != sep; ++i) {
    oss << "  ";
    oss.width(static_cast<std::streamsize>(name_width));
    oss << i->helptext << "  : " << i->text << std::endl;
  }
  if (sep != last) {
    oss << std::endl;
    oss << "Application Options:" << std::endl;
    for (auto i = sep; i != last; ++i) {
      oss << "  ";
      oss.width(static_cast<std::streamsize>(name_width));
      oss << i->helptext << "  : " << i->text << std::endl;
    }
  }
  return oss.str();
}

actor_system_config& actor_system_config::parse(int argc, char** argv,
                                                const char* ini_file_cstr) {
  string_list args;
  if (argc > 1)
    args.assign(argv + 1, argv + argc);
  return parse(std::move(args), ini_file_cstr);
}

actor_system_config& actor_system_config::parse(int argc, char** argv,
                                                std::istream& ini) {
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

struct ini_sentinel { };

bool operator!=(ini_iter iter, ini_sentinel) {
  return !iter.ini->fail();
}

} // namespace <anonymous>

actor_system_config& actor_system_config::parse(string_list args,
                                                std::istream& ini) {
  // Insert possibly user-defined values into the map to respect overrides to
  // member variables.
  // TODO: remove with CAF 0.17
  for (auto& opt : custom_options_) {
    auto val = opt.get();
    if (val)
      content[opt.category()][opt.long_name()] = std::move(*val);
  }
  // Content of the INI file overrides hard-coded defaults.
  if (ini.good()) {
    detail::ini_consumer consumer{custom_options_, content};
    detail::parser::state<ini_iter, ini_sentinel> res;
    res.i = ini_iter{&ini};
    detail::parser::read_ini(res, consumer);
    if (res.i != res.e)
      std::cerr << "*** error in config file [line " << res.line << " col "
                << res.column << "]: " << to_string(res.code) << std::endl;
  }
  // CLI options override the content of the INI file.
  using std::make_move_iterator;
  auto res = custom_options_.parse(content, args);
  if (res.second != args.end()) {
    if (res.first != pec::success) {
      std::cerr << "error: at argument \"" << *res.second
                << "\": " << to_string(res.first) << std::endl;
      cli_helptext_printed = true;
    }
    auto first = args.begin();
    first += std::distance(args.cbegin(), res.second);
    remainder.insert(remainder.end(), make_move_iterator(first),
                     make_move_iterator(args.end()));
    args_remainder = message_builder{remainder.begin(), remainder.end()}
                     .move_to_message();
  } else {
    cli_helptext_printed = get_or(content, "global.help", false)
                           || get_or(content, "global.long-help", false);
  }
  // Generate help text if needed.
  if (cli_helptext_printed) {
    bool long_help = get_or(content, "global.long-help", false);
    std::cout << custom_options_.help_text(!long_help) << std::endl;
  }
  // Generate INI dump if needed.
  if (!cli_helptext_printed && get_or(content, "global.dump-config", false)) {
    for (auto& category : content) {
      std::cout << '[' << category.first << "]\n";
      for (auto& kvp : category.second)
        if (kvp.first != "dump-config")
          std::cout << kvp.first << '=' << to_string(kvp.second) << '\n';
    }
    std::cout << std::flush;
    cli_helptext_printed = true;
  }
  return *this;
}

actor_system_config& actor_system_config::parse(string_list args,
                                                const char* ini_file_cstr) {
  // Override default config file name if set by user.
  if (ini_file_cstr != nullptr)
    config_file_path = ini_file_cstr;
  // CLI arguments always win.
  extract_config_file_path(args);
  std::ifstream ini{config_file_path};
  return parse(std::move(args), ini);
}

actor_system_config& actor_system_config::parse(message& msg,
                                                const char* ini_file_cstr) {
  string_list args;
  for (size_t i = 0; i < msg.size(); ++i)
    if (msg.match_element<std::string>(i))
      args.emplace_back(msg.get_as<std::string>(i));
  return parse(std::move(args), ini_file_cstr);
}

actor_system_config& actor_system_config::parse(message& msg,
                                                std::istream& ini) {
  string_list args;
  for (size_t i = 0; i < msg.size(); ++i)
    if (msg.match_element<std::string>(i))
      args.emplace_back(msg.get_as<std::string>(i));
  return parse(std::move(args), ini);
}

actor_system_config&
actor_system_config::add_actor_factory(std::string name, actor_factory fun) {
  actor_factories.emplace(std::move(name), std::move(fun));
  return *this;
}

actor_system_config&
actor_system_config::add_error_category(atom_value x, error_renderer y) {
  error_renderers[x] = y;
  return *this;
}

actor_system_config& actor_system_config::set_impl(string_view name,
                                                   config_value value) {
  auto opt = custom_options_.qualified_name_lookup(name);
  if (opt != nullptr && opt->check(value) == none) {
    opt->store(value);
    content[opt->category()][opt->long_name()] = std::move(value);
  }
  return *this;
}

timespan actor_system_config::stream_tick_duration() const noexcept {
  auto ns_count = caf::detail::gcd(stream_credit_round_interval.count(),
                                   stream_max_batch_delay.count());
  return timespan{ns_count};
}

timespan actor_system_config::streaming_credit_round_interval() const noexcept {
  return stream_credit_round_interval;
}

std::string actor_system_config::render_sec(uint8_t x, atom_value,
                                            const message& xs) {
  auto tmp = static_cast<sec>(x);
  return deep_to_string(meta::type_name("system_error"), tmp,
                        meta::omittable_if_empty(), xs);
}

std::string actor_system_config::render_exit_reason(uint8_t x, atom_value,
                                                    const message& xs) {
  auto tmp = static_cast<exit_reason>(x);
  return deep_to_string(meta::type_name("exit_reason"), tmp,
                        meta::omittable_if_empty(), xs);
}

void actor_system_config::extract_config_file_path(string_list& args) {
  static constexpr const char needle[] = "--caf#config-file=";
  auto last = args.end();
  auto i = std::find_if(args.begin(), last, [](const std::string& arg) {
    return arg.compare(0, sizeof(needle) - 1, needle) == 0;
  });
  if (i == last)
    return;
  auto arg_begin = i->begin() + sizeof(needle);
  auto arg_end = i->end();
  if (arg_begin == arg_end) {
    // Missing value.
    // TODO: print warning?
    return;
  }
  if (*arg_begin == '"') {
    detail::parser::state<std::string::const_iterator> res;
    res.i = arg_begin;
    res.e = arg_end;
    struct consumer {
      std::string result;
      void value(std::string&& x) {
        result = std::move(x);
      }
    };
    consumer f;
    detail::parser::read_string(res, f);
    if (res.code == pec::success)
      config_file_path = std::move(f.result);
    // TODO: else print warning?
  } else {
    // We support unescaped strings for convenience on the CLI.
    config_file_path = std::string{arg_begin, arg_end};
  }
  args.erase(i);
}

const dictionary<dictionary<config_value>>&
content(const actor_system_config& cfg) {
  return cfg.content;
}

} // namespace caf

CAF_POP_WARNINGS
