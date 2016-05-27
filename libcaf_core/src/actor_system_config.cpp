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

using options_vector = actor_system_config::options_vector;

class actor_system_config_reader {
public:
  using sink = std::function<void (size_t, config_value&)>;

  actor_system_config_reader(options_vector& xs, options_vector& ys) {
    add_opts(xs);
    add_opts(ys);
  }

  void add_opts(options_vector& xs) {
    for (auto& x : xs) {
      std::string key = x->category();
      key += '.';
      // name can have format "<long>,<short>"; consider only long name
      auto name_begin = x->name();
      auto name_end = name_begin;
      for (auto c = *name_end; c != ',' && c != 0; ++name_end) {
        // nop
      }
      key.insert(key.end(), name_begin, name_end);
      //key += x->name();
      sinks_.emplace(std::move(key), x->to_sink());
    }
  }

  void operator()(size_t ln, std::string name, config_value& cv) {
    auto i = sinks_.find(name);
    if (i == sinks_.end())
      std::cerr << "error in line " << ln
                << ": unrecognized parameter name \"" << name << "\"";
    else
      (i->second)(ln, cv);
  }

private:
  std::map<std::string, sink> sinks_;
};

} // namespace <anonymous>

actor_system_config::opt_group::opt_group(options_vector& xs,
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
  middleman_network_backend = atom("default");
  middleman_enable_automatic_connections = false;
  middleman_max_consecutive_reads = 50;
  middleman_heartbeat_interval = 0;
  nexus_port = 0;
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
  opt_group{options_, "middleman"}
  .add(middleman_network_backend, "network-backend",
       "sets the network backend to either 'default' or 'asio' (if available)")
  .add(middleman_enable_automatic_connections, "enable-automatic-connections",
       "enables or disables automatic connection management (off per default)")
  .add(middleman_max_consecutive_reads, "max-consecutive-reads",
       "sets the maximum number of consecutive I/O reads per broker")
  .add(middleman_heartbeat_interval, "heartbeat-interval",
       "sets the interval (ms) of heartbeat, 0 (default) means disabling it");
  opt_group{options_, "probe"}
  .add(nexus_host, "nexus-host",
       "sets the hostname or IP address for connecting to the Nexus")
  .add(nexus_port, "nexus-port",
       "sets the port for connecting to the Nexus");
  opt_group(options_, "opencl")
  .add(opencl_device_ids, "device-ids",
       "restricts which OpenCL devices are accessed by CAF");
  // add renderers for default error categories
  error_renderers_.emplace(atom("system"), render_sec);
  error_renderers_.emplace(atom("exit"), render_exit_reason);
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
  init();
  auto args = message_builder(argv + 1, argv + argc).move_to_message();
  // extract config file name first, since INI files are overruled by CLI args
  std::string config_file_name;
  if (ini_file_cstr)
    config_file_name = ini_file_cstr;
  args.extract_opts({
    {"caf#config-file", "", config_file_name}
  });
  // (2) content of the INI file overrides hard-coded defaults
  if (! config_file_name.empty()) {
    std::ifstream ini{config_file_name};
    if (ini.good()) {
      actor_system_config_reader consumer{options_, custom_options_};
      detail::parse_ini(ini, consumer, std::cerr);
    }
  }
  // (3) CLI options override the content of the INI file
  std::vector<message::cli_arg> cargs;
  for (auto& x : options_)
    cargs.emplace_back(x->to_cli_arg(true));
  cargs.emplace_back("caf#dump-config", "print config in INI format to stdout");
  //cargs.emplace_back("caf#help", "print this text");
  cargs.emplace_back("caf#config-file", "parse INI file", config_file_name);
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
    options_vector* all_options[] = { &options_, &custom_options_ };
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
  actor_factories_.emplace(std::move(name), std::move(fun));
  return *this;
}

actor_system_config&
actor_system_config::add_error_category(atom_value x, error_renderer y) {
  error_renderers_.emplace(x, y);
  return *this;
}

actor_system_config&
actor_system_config::set(const char* config_name, config_value config_value) {
  std::string cn;
  for (auto& x : options_) {
    // config_name has format "$category.$name"
    cn = x->category();
    cn += '.';
    cn += x->name();
    if (cn == config_name) {
      auto f = x->to_sink();
      f(0, config_value);
    }
  }
  return *this;
}

void actor_system_config::init() {
  // nop
}

std::string actor_system_config::render_sec(uint8_t x, atom_value,
                                            const message&) {
  return "system_error" + deep_to_string_as_tuple(static_cast<sec>(x));
}

std::string actor_system_config::render_exit_reason(uint8_t x, atom_value,
                                                    const message&) {
  return "exit_reason" + deep_to_string_as_tuple(static_cast<exit_reason>(x));
}

} // namespace caf
