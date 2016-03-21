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

struct type_name_visitor : static_visitor<const char*> {
  const char* operator()(const std::string&) const {
    return "a string";
  }
  const char* operator()(double) const {
    return "a double";
  }
  const char* operator()(int64_t) const {
    return "an integer";
  }
  const char* operator()(size_t) const {
    return "an unsigned integer";
  }
  const char* operator()(uint16_t) const {
    return "an unsigned short integer";
  }
  const char* operator()(bool) const {
    return "a boolean";
  }
  const char* operator()(atom_value) const {
    return "an atom";
  }
};

template <class T, class U>
bool assign_config_value(T& x, U& y) {
  x = std::move(y);
  return true;
}

// 32-bit platforms
template <class T>
inline typename std::enable_if<sizeof(T) == sizeof(uint32_t), bool>::type
unsigned_assign_in_range(T&, int64_t& x) {
  return x <= std::numeric_limits<T>::max();
}

// 64-bit platforms
template <class T>
inline typename std::enable_if<sizeof(T) == sizeof(uint64_t), bool>::type
unsigned_assign_in_range(T&, int64_t&) {
  return true;
}

bool assign_config_value(size_t& x, int64_t& y) {
  if (y < 0 || ! unsigned_assign_in_range(x, y))
    return false;
  x = std::move(y);
  return true;
}

bool assign_config_value(uint16_t& x, int64_t& y) {
  if (y < 0 || y > std::numeric_limits<uint16_t>::max())
    return false;
  x = std::move(y);
  return true;
}

using options_vector = actor_system_config::options_vector;

class actor_system_config_reader {
public:
  using config_value = actor_system_config::config_value;

  using sink = std::function<void (size_t, config_value&)>;

  actor_system_config_reader(options_vector& xs) {
    for (auto& x : xs) {
      std::string key = x->category();
      key += '.';
      key += x->name();
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

using cstr = const char*;

template <class T>
void add_option(options_vector& xs,
                T& storage, cstr category, cstr name, cstr explanation) {
  using option = actor_system_config::option;
  using config_value = actor_system_config::config_value;
  class impl : public option {
  public:
    impl(cstr ctg, cstr nm, cstr xp, T& ref) : option(ctg, nm, xp), ref_(ref) {
      // nop
    }
    std::string to_string() const override {
      return deep_to_string(ref_);
    }
    message::cli_arg to_cli_arg() const override {
      std::string argname = "caf#";
      argname += category();
      argname += ".";
      argname += name();
      return {std::move(argname), explanation(), ref_};
    }
    config_reader_sink to_sink() const override {
      return [=](size_t ln, config_value& x) {
        // the INI parser accepts all integers as int64_t
        using cfg_type =
          typename std::conditional<
            std::is_integral<T>::value && ! std::is_same<bool, T>::value,
            int64_t,
            T
          >::type;
        if (get<cfg_type>(&x) && assign_config_value(ref_, get<cfg_type>(x)))
          return;
        type_name_visitor tnv;
        std::cerr << "error in line " << ln << ": expected "
                  << tnv(ref_) << " found "
                  << apply_visitor(tnv, x) << std::endl;
      };
    }
  private:
    T& ref_;
  };
  return xs.emplace_back(new impl(category, name, explanation, storage));
}

class opt_group {
public:
  opt_group(options_vector& xs, cstr category) : xs_(xs), category_(category) {
    // nop
  }

  template <class T>
  opt_group& add(T& storage, cstr option_name, cstr explanation) {
    add_option(xs_, storage, category_, option_name, explanation);
    return *this;
  }

private:
  options_vector& xs_;
  cstr category_;
};

} // namespace <anonymous>

actor_system_config::option::option(cstr ct, cstr nm, cstr xp)
    : category_(ct),
      name_(nm),
      explanation_(xp) {
  // nop
}

actor_system_config::option::~option() {
  // nop
}

// in this config class, we have (1) hard-coded defaults that are overridden
// by (2) INI-file contents that are in turn overridden by (3) CLI arguments

actor_system_config::actor_system_config()
    : cli_helptext_printed(false),
      slave_mode(false) {
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
  .add(nexus_port, "probe.nexus-port",
       "sets the port for connecting to the Nexus");
  opt_group(options_, "opencl")
  .add(opencl_device_ids, "device-ids",
       "restricts which OpenCL devices are accessed by CAF");
}

actor_system_config::actor_system_config(int argc, char** argv)
    : actor_system_config() {
  if (argc < 1)
    return;
  auto args = message_builder(argv + 1, argv + argc).move_to_message();
  // extract config file name first, since INI files are overruled by CLI args
  std::string config_file_name;
  args.extract_opts({
    {"caf#config-file", "", config_file_name}
  });
  // (2) content of the INI file overrides hard-coded defaults
  if (! config_file_name.empty()) {
    std::ifstream ini{config_file_name};
    if (ini.good()) {
      actor_system_config_reader consumer{options_};
      detail::parse_ini(ini, consumer, std::cerr);
    }
  }
  // (3) CLI options override the content of the INI file
  std::vector<message::cli_arg> cargs;
  for (auto& x : options_)
    cargs.emplace_back(x->to_cli_arg());
  cargs.emplace_back("caf#dump-config", "print config in INI format to stdout");
  cargs.emplace_back("caf#help", "print this text");
  cargs.emplace_back("caf#config-file", "parse INI file", config_file_name);
  cargs.emplace_back("caf#slave-mode", "run in slave mode");
  cargs.emplace_back("caf#slave-name", "set name for this slave", slave_name);
  cargs.emplace_back("caf#bootstrap-node", "set bootstrapping", bootstrap_node);
  auto res = args.extract_opts(std::move(cargs), nullptr, true);
  using std::cerr;
  using std::cout;
  using std::endl;
  if (res.opts.count("caf#help")) {
    cli_helptext_printed = true;
    cout << res.helptext << endl;
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
    cout << std::boolalpha
         << "[scheduler]" << endl
         << "policy=" << deep_to_string(scheduler_policy) << endl
         << "max-threads=" << scheduler_max_threads << endl
         << "max-throughput=" << scheduler_max_throughput << endl
         << "enable-profiling=" << scheduler_enable_profiling << endl;
    if (scheduler_enable_profiling)
      cout << "profiling-ms-resolution="
           << scheduler_profiling_ms_resolution << endl
           << "profiling_output_file="
           << deep_to_string(scheduler_profiling_output_file) << endl;
    cout << "[middleman]" << endl
         << "network-backend="
         << deep_to_string(middleman_network_backend) << endl
         << "enable-automatic-connections="
         << deep_to_string(middleman_enable_automatic_connections) << endl
         << "heartbeat-interval="
         << middleman_heartbeat_interval << endl;
  }
  args_remainder = std::move(res.remainder);
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

} // namespace caf
