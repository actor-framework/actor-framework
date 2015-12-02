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
  const char* operator()(const double) const {
    return "a double";
  }
  const char* operator()(const int64_t) const {
    return "an integer";
  }
  const char* operator()(const size_t) const {
    return "an unsigned integer";
  }
  const char* operator()(const bool) const {
    return "a boolean";
  }
  const char* operator()(const atom_value) const {
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

class actor_system_config_reader {
public:
  using config_value = actor_system_config::config_value;

  using sink = std::function<void (size_t, config_value&)>;

  template <class T>
  actor_system_config_reader& bind(std::string arg_name, T& storage) {
    auto fun = [&storage](size_t ln, config_value& x) {
      // the INI parser accepts all integers as int64_t
      using cfg_type =
        typename std::conditional<
          std::is_same<T, size_t>::value,
          int64_t,
          T
        >::type;
      if (get<cfg_type>(&x) && assign_config_value(storage, get<cfg_type>(x)))
        return;
      type_name_visitor tnv;
      std::cerr << "error in line " << ln << ": expected "
                << tnv(storage) << " found "
                << apply_visitor(tnv, x) << std::endl;
    };
    sinks_.emplace(std::move(arg_name), fun);
    return *this;
  }

  void operator()(size_t ln, std::string name, config_value& cv) {
    auto i = sinks_.find(name);
    if (i == sinks_.end())
      std::cerr << "error in line " << ln << ": unrecognized parameter name \""
                << name << "\"";
    else
      (i->second)(ln, cv);
  }

private:
  std::map<std::string, sink> sinks_;
};

} // namespace <anonymous>

actor_system_config::actor_system_config() {
  // (1) hard-coded defaults
  scheduler_policy = atom("stealing");
  scheduler_max_threads = std::max(std::thread::hardware_concurrency(),
                                   unsigned{4});
  scheduler_max_throughput = std::numeric_limits<size_t>::max();
  scheduler_enable_profiling = false;
  scheduler_profiling_ms_resolution = 100;
  middleman_network_backend = atom("default");
  middleman_enable_automatic_connections = false;
}

actor_system_config::actor_system_config(int argc, char** argv)
    : actor_system_config() {
  if (argc < 2)
    return;
  auto args = message_builder(argv, argv + argc).move_to_message();
  std::string config_file_name;
  args.extract_opts({
    {"caf-config-file", "", config_file_name}
  });
  // (2) content of the INI file overrides hard-coded defaults
  if (! config_file_name.empty()) {
    std::ifstream ini{config_file_name};
    if (ini.good()) {
      actor_system_config_reader consumer;
      consumer.bind("scheduler.policy", scheduler_policy)
              .bind("scheduler.scheduler-max-threads", scheduler_max_threads)
              .bind("scheduler.max-throughput", scheduler_max_throughput)
              .bind("scheduler.enable-profiling", scheduler_enable_profiling)
              .bind("scheduler.profiling-ms-resolution",
                    scheduler_profiling_ms_resolution)
              .bind("scheduler.profiling-output-file",
                    scheduler_profiling_output_file)
              .bind("middleman.network-backend", middleman_network_backend)
              .bind("middleman.enable-automatic-connections",
                    middleman_enable_automatic_connections);
      detail::parse_ini(ini, consumer, std::cerr);
    }
  }
  // (3) CLI options override the content of the INI file
  auto res = args.extract_opts({
    {"caf#scheduler.policy",
     "sets the scheduling policy to either 'stealing' (default) or 'sharing'",
     scheduler_policy},
    {"caf#scheduler.scheduler-max-threads",
     "sets a fixed number of worker threads for the scheduler",
     scheduler_max_threads},
    {"caf#scheduler.max-throughput",
     "sets the maximum number of messages an actor consumes before yielding",
     scheduler_max_throughput},
    {"caf#scheduler.enable-profiling",
     "enables or disables profiler output",
     scheduler_enable_profiling},
    {"caf#scheduler.profiling-ms-resolution",
     "sets the rate in ms in which the profiler collects data",
     scheduler_profiling_ms_resolution},
    {"caf#scheduler.profiling-output-file",
     "sets the output file for the profiler",
     scheduler_profiling_output_file},
    {"caf#middleman.network-backend",
     "sets the network backend to either 'default' or 'asio' (if available)",
     middleman_network_backend},
    {"caf#middleman.enable-automatic-connections",
     "enables or disables automatic connection management (off per default)",
     middleman_enable_automatic_connections},
    {"caf-dump-config", "print config in INI format to stdout"},
    {"caf-help", "print this text"}
  }, nullptr, true);
  using std::cerr;
  using std::cout;
  using std::endl;
  if (res.opts.count("caf-help"))
    cout << res.helptext << endl;
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
  if (res.opts.count("caf-dump-config")) {
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
         << deep_to_string(middleman_enable_automatic_connections) << endl;
  }
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

} // namespace caf
