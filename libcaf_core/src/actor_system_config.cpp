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

#include "caf/message_builder.hpp"

#include "caf/detail/gcd.hpp"
#include "caf/detail/parse_ini.hpp"

namespace caf {

namespace {

using option_vector = actor_system_config::option_vector;
const char actor_conf_prefix[] = "actor:";
constexpr size_t actor_conf_prefix_size = 6;

class actor_system_config_reader {
public:
  using sink = std::function<void (size_t, config_value&,
                                   optional<std::ostream&>)>;

  using named_actor_sink = std::function<void (size_t, const std::string&,
                                               config_value&)>;

  actor_system_config_reader(option_vector& xs, option_vector& ys,
                             named_actor_sink na_sink)
      : named_actor_sink_(std::move(na_sink)){
    add_opts(xs);
    add_opts(ys);
  }

  void add_opts(option_vector& xs) {
    for (auto& x : xs)
      sinks_.emplace(x->full_name(), x->to_sink());
  }

  void operator()(size_t ln, const std::string& name, config_value& cv,
                  optional<std::ostream&> out) {
    auto i = sinks_.find(name);
    if (i != sinks_.end()) {
      (i->second)(ln, cv, none);
      return;
    }
    // check whether this is an individual actor config
    if (name.compare(0, actor_conf_prefix_size, actor_conf_prefix) == 0) {
      auto substr = name.substr(actor_conf_prefix_size);
      named_actor_sink_(ln, substr, cv);
      return;
    }
    if (out)
        *out << "error in line " << ln
             << R"(: unrecognized parameter name ")" << name << R"(")"
             << std::endl;
  }

private:
  std::map<std::string, sink> sinks_;
  named_actor_sink named_actor_sink_;
};

} // namespace <anonymous>

actor_system_config::opt_group::opt_group(option_vector& xs,
                                          const char* category)
    : xs_(xs),
      cat_(category) {
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
  message args;
  if (argc > 1)
    args = message_builder(argv + 1, argv + argc).move_to_message();
  return parse(args, ini_file_cstr);
}

actor_system_config& actor_system_config::parse(int argc, char** argv,
                                                std::istream& ini) {
  message args;
  if (argc > 1)
    args = message_builder(argv + 1, argv + argc).move_to_message();
  return parse(args, ini);
}

actor_system_config& actor_system_config::parse(message& args,
                                                const char* ini_file_cstr) {
  // Override default config file name if set by user.
  if (ini_file_cstr != nullptr)
    config_file_path = ini_file_cstr;
  // CLI arguments always win.
  extract_config_file_path(args);
  std::ifstream ini{config_file_path};
  return parse(args, ini);
}

actor_system_config& actor_system_config::parse(message& args,
                                                std::istream& ini) {
  // (2) content of the INI file overrides hard-coded defaults
  if (ini.good()) {
    using conf_sink = std::function<void (size_t, config_value&,
                                          optional<std::ostream&>)>;
    using conf_sinks = std::unordered_map<std::string, conf_sink>;
    using conf_mapping = std::pair<option_vector, conf_sinks>;
    hash_map<std::string, conf_mapping> ovs;
    auto nac_sink = [&](size_t ln, const std::string& nm, config_value& cv) {
      std::string actor_name{nm.begin(), std::find(nm.begin(), nm.end(), '.')};
      auto ac = named_actor_configs.find(actor_name);
      if (ac == named_actor_configs.end())
        ac = named_actor_configs.emplace(actor_name,
                                         named_actor_config{}).first;
      auto& ov = ovs[actor_name];
      if (ov.first.empty()) {
        opt_group(ov.first, ac->first.c_str())
        .add(ac->second.strategy, "strategy", "")
        .add(ac->second.low_watermark, "low-watermark", "")
        .add(ac->second.max_pending, "max-pending", "");
        for (auto& opt : ov.first)
          ov.second.emplace(opt->full_name(), opt->to_sink());
      }
      auto i = ov.second.find(nm);
      if (i != ov.second.end())
        i->second(ln, cv, none);
      else
        std::cerr << "error in line " << ln
                  << R"(: unrecognized parameter name ")" << nm << R"(")"
                  << std::endl;
    };
    actor_system_config_reader consumer{options_, custom_options_, nac_sink};
    detail::parse_ini(ini, consumer, std::cerr);
  }
  // (3) CLI options override the content of the INI file
  std::string dummy; // caf#config-file either ignored or already open
  std::vector<message::cli_arg> cargs;
  for (auto& x : options_)
    cargs.emplace_back(x->to_cli_arg(true));
  cargs.emplace_back("caf#dump-config", "print config in INI format to stdout");
  //cargs.emplace_back("caf#help", "print this text");
  cargs.emplace_back("caf#config-file", "parse INI file", dummy);
  cargs.emplace_back("caf#slave-mode", "run in slave mode");
  cargs.emplace_back("caf#slave-name", "set name for this slave", slave_name);
  cargs.emplace_back("caf#bootstrap-node", "set bootstrapping", bootstrap_node);
  for (auto& x : custom_options_)
    cargs.emplace_back(x->to_cli_arg(false));
  using std::placeholders::_1;
  auto res = args.extract_opts(std::move(cargs),
                               std::bind(&actor_system_config::make_help_text,
                                         this, _1));
  using std::cerr;
  using std::cout;
  using std::endl;
  args_remainder = std::move(res.remainder);
  if (!res.error.empty()) {
    cli_helptext_printed = true;
    std::cerr << res.error << endl;
    return *this;
  }
  if (res.opts.count("help") != 0u) {
    cli_helptext_printed = true;
    cout << res.helptext << endl;
    return *this;
  }
  if (res.opts.count("caf#slave-mode") != 0u) {
    slave_mode = true;
    if (slave_name.empty())
      std::cerr << "running in slave mode but no name was configured" << endl;
    if (bootstrap_node.empty())
      std::cerr << "running in slave mode without bootstrap node" << endl;
  }
  // Verify settings.
  auto verify_atom_opt = [](std::initializer_list<atom_value> xs, atom_value& x,
                            const char* xname) {
    if (std::find(xs.begin(), xs.end(), x) == xs.end()) {
      cerr << "[WARNING] invalid value for " << xname
           << " defined, falling back to "
           << deep_to_string(*xs.begin()) << endl;
      x = *xs.begin();
    }
  };
  verify_atom_opt({atom("default"),
#                  ifdef CAF_USE_ASIO
                   atom("asio")
#                  endif
                  }, middleman_network_backend, "middleman.network-backend");
  verify_atom_opt({atom("stealing"), atom("sharing"), atom("testing")},
                  scheduler_policy, "scheduler.policy ");
  if (res.opts.count("caf#dump-config") != 0u) {
    cli_helptext_printed = true;
    std::string category;
    for_each_option([&](const config_option& x) {
      if (category != x.category()) {
        category = x.category();
        cout << "[" << category << "]" << endl;
      }
      cout << x.name() << "=" << x.to_string() << endl;
    });
  }
  return *this;
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

actor_system_config& actor_system_config::set_impl(const char* cn,
                                                   config_value cv) {
  auto e = options_.end();
  auto i = std::find_if(options_.begin(), e, [cn](const option_ptr& ptr) {
    return ptr->full_name() == cn;
  });
  if (i != e) {
    auto f = (*i)->to_sink();
    f(0, cv, none);
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

void actor_system_config::extract_config_file_path(message& args) {
  auto res = args.extract_opts({
    {"caf#config-file", "", config_file_path}
  });
  args = res.remainder;
}

} // namespace caf
