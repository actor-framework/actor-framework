/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
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

#include "caf/actor_system_config.hpp"

#include <limits>
#include <thread>
#include <fstream>
#include <sstream>

#include "caf/message_builder.hpp"

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
      logger_filename(logger_file_name),
      logger_filter(logger_component_filter),
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
  scheduler_policy = atom("numa-steal");
  scheduler_max_threads = std::max(std::thread::hardware_concurrency(),
                                   unsigned{4});
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
  numa_aware_work_stealing_neighborhood_level = 1;
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
  middleman_detach_multiplexer = true;
  // fill our options vector for creating INI and CLI parsers
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
  opt_group{options_, "numa"}
  .add(numa_aware_work_stealing_neighborhood_level, "neighborhood-level",
       "defines the neighborhood radius (0=all, 1=next smaller group, 2=...)");
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
       "enables or disables background activity of the multiplexer");
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

actor_system_config::actor_system_config(actor_system_config&& other)
    : cli_helptext_printed(other.cli_helptext_printed),
      slave_mode(other.slave_mode),
      slave_name(std::move(other.slave_name)),
      bootstrap_node(std::move(other.bootstrap_node)),
      args_remainder(std::move(other.args_remainder)),
      scheduler_policy(other.scheduler_policy),
      scheduler_max_threads(other.scheduler_max_threads),
      scheduler_max_throughput(other.scheduler_max_throughput),
      scheduler_enable_profiling(std::move(other.scheduler_enable_profiling)),
      scheduler_profiling_ms_resolution(
        other.scheduler_profiling_ms_resolution),
      scheduler_profiling_output_file(other.scheduler_profiling_output_file),
      work_stealing_aggressive_poll_attempts(
        other.work_stealing_aggressive_poll_attempts),
      work_stealing_aggressive_steal_interval(
        other.work_stealing_aggressive_steal_interval),
      work_stealing_moderate_poll_attempts(
        other.work_stealing_moderate_poll_attempts),
      work_stealing_moderate_steal_interval(
        other.work_stealing_moderate_steal_interval),
      work_stealing_moderate_sleep_duration_us(
        other.work_stealing_moderate_sleep_duration_us),
      work_stealing_relaxed_steal_interval(
        other.work_stealing_relaxed_steal_interval),
      work_stealing_relaxed_sleep_duration_us(
        other.work_stealing_relaxed_sleep_duration_us),
      logger_file_name(std::move(other.logger_file_name)),
      logger_file_format(std::move(other.logger_file_format)),
      logger_console(other.logger_console),
      logger_console_format(std::move(other.logger_console_format)),
      logger_component_filter(std::move(other.logger_component_filter)),
      logger_verbosity(other.logger_verbosity),
      logger_inline_output(other.logger_inline_output),
      logger_filename(logger_file_name),
      logger_filter(logger_component_filter),
      middleman_network_backend(other.middleman_network_backend),
      middleman_app_identifier(std::move(other.middleman_app_identifier)),
      middleman_enable_automatic_connections(
        other.middleman_enable_automatic_connections),
      middleman_max_consecutive_reads(other.middleman_max_consecutive_reads),
      middleman_heartbeat_interval(other.middleman_heartbeat_interval),
      middleman_detach_utility_actors(other.middleman_detach_utility_actors),
      middleman_detach_multiplexer(other.middleman_detach_multiplexer),
      opencl_device_ids(std::move(other.opencl_device_ids)),
      openssl_certificate(std::move(other.openssl_certificate)),
      openssl_key(std::move(other.openssl_key)),
      openssl_passphrase(std::move(other.openssl_passphrase)),
      openssl_capath(std::move(other.openssl_capath)),
      openssl_cafile(std::move(other.openssl_cafile)),
      value_factories_by_name(std::move(other.value_factories_by_name)),
      value_factories_by_rtti(std::move(other.value_factories_by_rtti)),
      actor_factories(std::move(other.actor_factories)),
      module_factories(std::move(other.module_factories)),
      hook_factories(std::move(other.hook_factories)),
      group_module_factories(std::move(other.group_module_factories)),
      type_names_by_rtti(std::move(other.type_names_by_rtti)),
      error_renderers(std::move(other.error_renderers)),
      named_actor_configs(std::move(other.named_actor_configs)),
      slave_mode_fun(other.slave_mode_fun),
      custom_options_(std::move(other.custom_options_)),
      options_(std::move(other.options_)) {
  // nop
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
  // set default config file name if not set by user
  if (ini_file_cstr == nullptr)
    ini_file_cstr = "caf-application.ini";
  std::string config_file_name;
  // CLI file name has priority over default file name
  args.extract_opts({
    {"caf#config-file", "", config_file_name}
  });
  if (config_file_name.empty())
    config_file_name = ini_file_cstr;
  std::ifstream ini{config_file_name};
  return parse(args, ini);
}

actor_system_config& actor_system_config::parse(int argc, char** argv,
                                                std::istream& ini) {
  message args;
  if (argc > 1)
    args = message_builder(argv + 1, argv + argc).move_to_message();
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
  verify_atom_opt({atom("stealing"), atom("sharing"), atom("numa-steal")},
                  scheduler_policy, "scheduler.policy ");
  if (res.opts.count("caf#dump-config") != 0u) {
    cli_helptext_printed = true;
    std::string category;
    option_vector* all_options[] = { &options_, &custom_options_ };
    for (auto& opt_vec : all_options) {
      for (auto& opt : *opt_vec) {
        if (category != opt->category()) {
          category = opt->category();
          cout << "[" << category << "]" << endl;
        }
        cout << opt->name() << "=" << opt->to_string() << endl;
      }
    }
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

actor_system_config& actor_system_config::set(const char* cn, config_value cv) {
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

} // namespace caf
