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

#include "caf/detail/gcd.hpp"
#include "caf/detail/ini_consumer.hpp"
#include "caf/detail/parser/read_ini.hpp"
#include "caf/detail/parser/read_string.hpp"
#include "caf/message_builder.hpp"

namespace caf {

actor_system_config::opt_group::opt_group(config_option_set& xs,
                                          const char* category)
    : xs_(xs),
      category_(category) {
  // nop
}

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
  streaming_desired_batch_complexity_us = 50;
  streaming_max_batch_delay_us = 50000;
  streaming_credit_round_interval_us = 100000;
  scheduler_policy = atom("stealing");
  scheduler_max_threads = std::max(std::thread::hardware_concurrency(), 4u);
  scheduler_max_throughput = std::numeric_limits<size_t>::max();
  scheduler_enable_profiling = false;
  scheduler_profiling_ms_resolution = 100;
  work_stealing_aggressive_poll_attempts = 100;
  work_stealing_aggressive_steal_interval = 10;
  work_stealing_moderate_poll_attempts = 500;
  work_stealing_moderate_steal_interval = 5;
  work_stealing_moderate_sleep_duration_us = 50;
  work_stealing_relaxed_steal_interval = 1;
  work_stealing_relaxed_sleep_duration_us = 10000;
  logger_file_name = "actor_log_[PID]_[TIMESTAMP]_[NODE].log";
  logger_file_format = "%r %c %p %a %t %C %M %F:%L %m%n";
  logger_console = atom("none");
  logger_console_format = "%m";
  logger_verbosity = atom("trace");
  logger_inline_output = false;
  middleman_network_backend = atom("default");
  middleman_enable_automatic_connections = false;
  middleman_max_consecutive_reads = 50;
  middleman_heartbeat_interval = 0;
  middleman_detach_utility_actors = true;
  middleman_detach_multiplexer = true;
  middleman_enable_tcp = true;
  middleman_enable_udp = false;
  middleman_cached_udp_buffers = 10;
  middleman_max_pending_msgs = 10;
  // fill our options vector for creating INI and CLI parsers
  opt_group{options_, "streaming"}
  .add(streaming_desired_batch_complexity_us, "desired-batch-complexity-us",
       "sets the desired timespan for a single batch")
  .add(streaming_max_batch_delay_us, "max-batch-delay-us",
       "sets the maximum delay for sending underfull batches in microseconds")
  .add(streaming_credit_round_interval_us, "credit-round-interval-us",
       "sets the length of credit intervals in microseconds");
  opt_group{options_, "scheduler"}
  .add(scheduler_policy, "policy",
       "sets the scheduling policy to either 'stealing' (default) or 'sharing'")
  .add(scheduler_max_threads, "max-threads",
       "sets a fixed number of worker threads for the scheduler")
  .add(scheduler_max_throughput, "max-throughput",
       "sets the maximum number of messages an actor consumes before yielding")
  .add(scheduler_enable_profiling, "enable-profiling",
       "enables or disables profiler output")
  .add(scheduler_profiling_ms_resolution, "profiling-ms-resolution",
       "sets the rate in ms in which the profiler collects data")
  .add(scheduler_profiling_output_file, "profiling-output-file",
       "sets the output file for the profiler");
  opt_group(options_, "work-stealing")
  .add(work_stealing_aggressive_poll_attempts, "aggressive-poll-attempts",
       "sets the number of zero-sleep-interval polling attempts")
  .add(work_stealing_aggressive_steal_interval, "aggressive-steal-interval",
       "sets the frequency of steal attempts during aggressive polling")
  .add(work_stealing_moderate_poll_attempts, "moderate-poll-attempts",
       "sets the number of moderately aggressive polling attempts")
  .add(work_stealing_moderate_steal_interval, "moderate-steal-interval",
       "sets the frequency of steal attempts during moderate polling")
  .add(work_stealing_moderate_sleep_duration_us, "moderate-sleep-duration",
       "sets the sleep interval between poll attempts during moderate polling")
  .add(work_stealing_relaxed_steal_interval, "relaxed-steal-interval",
       "sets the frequency of steal attempts during relaxed polling")
  .add(work_stealing_relaxed_sleep_duration_us, "relaxed-sleep-duration",
       "sets the sleep interval between poll attempts during relaxed polling");
  opt_group{options_, "logger"}
  .add(logger_file_name, "file-name",
       "sets the filesystem path of the log file")
  .add(logger_file_format, "file-format",
       "sets the line format for individual log file entires")
  .add(logger_console, "console",
       "sets the type of output to std::clog (none|colored|uncolored)")
  .add(logger_console_format, "console-format",
       "sets the line format for printing individual log entires")
  .add(logger_component_filter, "component-filter",
       "exclude all listed components from logging")
  .add(logger_verbosity, "verbosity",
       "sets the verbosity (quiet|error|warning|info|debug|trace)")
  .add(logger_inline_output, "inline-output",
       "sets whether a separate thread is used for I/O")
  .add(logger_file_name, "filename",
       "deprecated (use file-name instead)")
  .add(logger_component_filter, "filter",
       "deprecated (use console-component-filter instead)");
  opt_group{options_, "middleman"}
  .add(middleman_network_backend, "network-backend",
       "sets the network backend to either 'default' or 'asio' (if available)")
  .add(middleman_app_identifier, "app-identifier",
       "sets the application identifier of this node")
  .add(middleman_enable_automatic_connections, "enable-automatic-connections",
       "enables or disables automatic connection management (off per default)")
  .add(middleman_max_consecutive_reads, "max-consecutive-reads",
       "sets the maximum number of consecutive I/O reads per broker")
  .add(middleman_heartbeat_interval, "heartbeat-interval",
       "sets the interval (ms) of heartbeat, 0 (default) means disabling it")
  .add(middleman_detach_utility_actors, "detach-utility-actors",
       "enables or disables detaching of utility actors")
  .add(middleman_detach_multiplexer, "detach-multiplexer",
       "enables or disables background activity of the multiplexer")
  .add(middleman_enable_tcp, "enable-tcp",
       "enable communication via TCP (on by default)")
  .add(middleman_enable_udp, "enable-udp",
       "enable communication via UDP (off by default)")
  .add(middleman_cached_udp_buffers, "cached-udp-buffers",
       "sets the max number of UDP send buffers that will be cached for reuse "
       "(default: 10)")
  .add(middleman_max_pending_msgs, "max-pending-messages",
       "sets the max number of UDP pending messages due to ordering "
       "(default: 10)");
  opt_group(options_, "opencl")
  .add(opencl_device_ids, "device-ids",
       "restricts which OpenCL devices are accessed by CAF");
  opt_group(options_, "openssl")
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
  if (argc < 2)
    return *this;
  string_list args{argv + 1, argv + argc};
  return parse(std::move(args), ini_file_cstr);
}

actor_system_config& actor_system_config::parse(int argc, char** argv,
                                                std::istream& ini) {
  if (argc < 2)
    return *this;
  string_list args{argv + 1, argv + argc};
  return parse(std::move(args), ini);
}

actor_system_config& actor_system_config::parse(string_list args,
                                                std::istream& ini) {
  // (2) content of the INI file overrides hard-coded defaults
  if (ini.good()) {
    detail::ini_consumer consumer{options_, content};
    detail::parser::state<std::istream_iterator<char>> res;
    res.i = std::istream_iterator<char>{ini};
    detail::parser::read_ini(res, consumer);
  }
  // (3) CLI options override the content of the INI file
  options_.parse(content, args);
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

actor_system_config& actor_system_config::set_impl(const char* name,
                                                   config_value value) {
  auto opt = options_.qualified_name_lookup(name);
  if (opt != nullptr && opt->check(value) == none) {
    opt->store(value);
    content[opt->category()][name] = std::move(value);
  }
  return *this;
}

size_t actor_system_config::streaming_tick_duration_us() const noexcept {
  return caf::detail::gcd(streaming_credit_round_interval_us,
                          streaming_max_batch_delay_us);
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

const std::map<std::string, std::map<std::string, config_value>>&
content(const actor_system_config& cfg) {
  return cfg.content;
}

} // namespace caf
