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

#include "caf/actor_system_config.hpp"

#include <limits>
#include <thread>
#include <fstream>

#include "caf/message_builder.hpp"

#include "caf/detail/parse_ini.hpp"

namespace caf {

namespace {

using option_vector = actor_system_config::option_vector;

class actor_system_config_reader {
public:
  using sink = std::function<void (size_t, config_value&)>;

  actor_system_config_reader(option_vector& xs, option_vector& ys) {
    add_opts(xs);
    add_opts(ys);
  }

  void add_opts(option_vector& xs) {
    for (auto& x : xs)
      sinks_.emplace(x->full_name(), x->to_sink());
  }

  void operator()(size_t ln, std::string name, config_value& cv) {
    auto i = sinks_.find(name);
    if (i != sinks_.end())
      (i->second)(ln, cv);
    else
      std::cerr << "error in line " << ln
                << ": unrecognized parameter name \"" << name << "\"";
  }

private:
  std::map<std::string, sink> sinks_;
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
      slave_mode_fun(nullptr) {
  // (1) hard-coded defaults
  scheduler_policy = atom("stealing");
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
  middleman_network_backend = atom("default");
  middleman_enable_automatic_connections = false;
  middleman_max_consecutive_reads = 50;
  middleman_heartbeat_interval = 0;
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
       "sets the interval (ms) of heartbeat, 0 (default) means disabling it");
  opt_group(options_, "opencl")
  .add(opencl_device_ids, "device-ids",
       "restricts which OpenCL devices are accessed by CAF");
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
  // set default config file name if not set by user
  if (! ini_file_cstr)
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
    actor_system_config_reader consumer{options_, custom_options_};
    auto f = [&](size_t ln, std::string str, config_value& x) {
      consumer(ln, std::move(str), x);
    };
    detail::parse_ini(ini, f, std::cerr);
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
  if (! res.error.empty()) {
    cli_helptext_printed = true;
    std::cerr << res.error << endl;
    return *this;
  }
  if (res.opts.count("help")) {
    cli_helptext_printed = true;
    cout << res.helptext << endl;
    return *this;
  }
  if (res.opts.count("caf#slave-mode")) {
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
  verify_atom_opt({atom("stealing"), atom("sharing")},
                  scheduler_policy, "scheduler.policy ");
  if (res.opts.count("caf#dump-config")) {
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
  error_renderers.emplace(x, y);
  return *this;
}

actor_system_config& actor_system_config::set(const char* cn, config_value cv) {
  std::string full_name;
  for (auto& x : options_) {
    // config_name has format "$category.$name"
    full_name = x->category();
    full_name += '.';
    full_name += x->name();
    if (full_name == cn) {
      auto f = x->to_sink();
      f(0, cv);
    }
  }
  return *this;
}

std::string actor_system_config::render_sec(uint8_t x, atom_value,
                                            const message& xs) {
  return "system_error"
         + (xs.empty() ? deep_to_string_as_tuple(static_cast<sec>(x))
                       : deep_to_string_as_tuple(static_cast<sec>(x), xs));
}

std::string actor_system_config::render_exit_reason(uint8_t x, atom_value,
                                                    const message&) {
  return "exit_reason" + deep_to_string_as_tuple(static_cast<exit_reason>(x));
}

} // namespace caf
