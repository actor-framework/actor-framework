// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/actor_system_config.hpp"

#include <fstream>
#include <limits>
#include <sstream>
#include <thread>

#include "caf/config.hpp"
#include "caf/config_option.hpp"
#include "caf/config_option_adder.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/config_consumer.hpp"
#include "caf/detail/gcd.hpp"
#include "caf/detail/parser/read_config.hpp"
#include "caf/detail/parser/read_string.hpp"
#include "caf/message_builder.hpp"
#include "caf/pec.hpp"
#include "caf/sec.hpp"
#include "caf/type_id.hpp"

namespace caf {

namespace {

constexpr const char* default_config_file = "$DEFAULT";

} // namespace

actor_system_config::~actor_system_config() {
  // nop
}

// in this config class, we have (1) hard-coded defaults that are overridden
// by (2) config file contents that are in turn overridden by (3) CLI arguments

actor_system_config::actor_system_config()
  : cli_helptext_printed(false),
    program_name("unknown-caf-app"),
    slave_mode(false),
    config_file_path(default_config_file),
    slave_mode_fun(nullptr) {
  // (1) hard-coded defaults
  stream_max_batch_delay = defaults::stream::max_batch_delay;
  stream_credit_round_interval = 2 * stream_max_batch_delay;
  // fill our options vector for creating config file and CLI parsers
  using std::string;
  using string_list = std::vector<string>;
  opt_group{custom_options_, "global"}
    .add<bool>("help,h?", "print help text to STDERR and exit")
    .add<bool>("long-help", "print long help text to STDERR and exit")
    .add<bool>("dump-config", "print configuration to STDERR and exit")
    .add<string>(config_file_path, "config-file",
                 "set config file (default: caf-application.conf)");
  opt_group{custom_options_, "caf.stream"}
    .add<timespan>(stream_max_batch_delay, "max-batch-delay",
                   "maximum delay for partial batches")
    .add<string>("credit-policy",
                 "selects an implementation for credit computation");
  opt_group{custom_options_, "caf.stream.size-based-policy"}
    .add<int32_t>("bytes-per-batch", "desired batch size in bytes")
    .add<int32_t>("buffer-capacity", "maximum input buffer size in bytes")
    .add<int32_t>("sampling-rate", "frequency of collecting batch sizes")
    .add<int32_t>("calibration-interval", "frequency of re-calibrations")
    .add<float>("smoothing-factor", "factor for discounting older samples");
  opt_group{custom_options_, "caf.stream.token-based-policy"}
    .add<int32_t>("batch-size", "number of elements per batch")
    .add<int32_t>("buffer-size", "max. number of elements in the input buffer");
  opt_group{custom_options_, "caf.scheduler"}
    .add<string>("policy", "'stealing' (default) or 'sharing'")
    .add<size_t>("max-threads", "maximum number of worker threads")
    .add<size_t>("max-throughput", "nr. of messages actors can consume per run")
    .add<bool>("enable-profiling", "enables profiler output")
    .add<timespan>("profiling-resolution", "data collection rate")
    .add<string>("profiling-output-file", "output file for the profiler");
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
  opt_group{custom_options_, "caf.logger"} //
    .add<bool>("inline-output", "disable logger thread (for testing only!)");
  opt_group{custom_options_, "caf.logger.file"}
    .add<string>("path", "filesystem path for the log file")
    .add<string>("format", "format for individual log file entries")
    .add<string>("verbosity", "minimum severity level for file output")
    .add<string_list>("excluded-components", "excluded components in files");
  opt_group{custom_options_, "caf.logger.console"}
    .add<bool>("colored", "forces colored or uncolored output")
    .add<string>("format", "format for printed console lines")
    .add<string>("verbosity", "minimum severity level for console output")
    .add<string_list>("excluded-components", "excluded components on console");
}

settings actor_system_config::dump_content() const {
  settings result = content;
  auto& caf_group = result["caf"].as_dictionary();
  // -- streaming parameters
  auto& stream_group = caf_group["stream"].as_dictionary();
  put_missing(stream_group, "max-batch-delay",
              defaults::stream::max_batch_delay);
  put_missing(stream_group, "credit-policy", defaults::stream::credit_policy);
  put_missing(stream_group, "size-policy.buffer-capacity",
              defaults::stream::size_policy::buffer_capacity);
  put_missing(stream_group, "size-policy.bytes-per-batch",
              defaults::stream::size_policy::bytes_per_batch);
  // -- scheduler parameters
  auto& scheduler_group = caf_group["scheduler"].as_dictionary();
  put_missing(scheduler_group, "policy", defaults::scheduler::policy);
  put_missing(scheduler_group, "max-throughput",
              defaults::scheduler::max_throughput);
  put_missing(scheduler_group, "enable-profiling", false);
  put_missing(scheduler_group, "profiling-resolution",
              defaults::scheduler::profiling_resolution);
  put_missing(scheduler_group, "profiling-output-file", std::string{});
  // -- work-stealing parameters
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
  // -- logger parameters
  auto& logger_group = caf_group["logger"].as_dictionary();
  put_missing(logger_group, "inline-output", false);
  auto& file_group = logger_group["file"].as_dictionary();
  put_missing(file_group, "path", defaults::logger::file::path);
  put_missing(file_group, "format", defaults::logger::file::format);
  put_missing(file_group, "excluded-components", std::vector<std::string>{});
  auto& console_group = logger_group["console"].as_dictionary();
  put_missing(console_group, "colored", defaults::logger::console::colored);
  put_missing(console_group, "format", defaults::logger::console::format);
  put_missing(console_group, "excluded-components", std::vector<std::string>{});
  // -- middleman parameters
  auto& middleman_group = caf_group["middleman"].as_dictionary();
  auto default_id = to_string(defaults::middleman::app_identifier);
  put_missing(middleman_group, "app-identifiers",
              std::vector<std::string>{std::move(default_id)});
  put_missing(middleman_group, "enable-automatic-connections", false);
  put_missing(middleman_group, "max-consecutive-reads",
              defaults::middleman::max_consecutive_reads);
  put_missing(middleman_group, "heartbeat-interval",
              defaults::middleman::heartbeat_interval);
  // -- openssl parameters
  auto& openssl_group = caf_group["openssl"].as_dictionary();
  put_missing(openssl_group, "certificate", std::string{});
  put_missing(openssl_group, "key", std::string{});
  put_missing(openssl_group, "passphrase", std::string{});
  put_missing(openssl_group, "capath", std::string{});
  put_missing(openssl_group, "cafile", std::string{});
  return result;
}

error actor_system_config::parse(int argc, char** argv,
                                 const char* config_file_cstr) {
  string_list args;
  if (argc > 0) {
    program_name = argv[0];
    if (argc > 1)
      args.assign(argv + 1, argv + argc);
  }
  return parse(std::move(args), config_file_cstr);
}

error actor_system_config::parse(int argc, char** argv, std::istream& conf) {
  string_list args;
  if (argc > 0) {
    program_name = argv[0];
    if (argc > 1)
      args.assign(argv + 1, argv + argc);
  }
  return parse(std::move(args), conf);
}

std::pair<int, char**> actor_system_config::c_args_remainder() const noexcept {
  return {static_cast<int>(c_args_remainder_.size()), c_args_remainder_.data()};
}

void actor_system_config::set_remainder(string_list args) {
  remainder.swap(args);
  c_args_remainder_buf_.assign(program_name.begin(), program_name.end());
  c_args_remainder_buf_.emplace_back('\0');
  for (const auto& arg : remainder) {
    c_args_remainder_buf_.insert(c_args_remainder_buf_.end(), //
                                 arg.begin(), arg.end());
    c_args_remainder_buf_.emplace_back('\0');
  }
  auto ptr = c_args_remainder_buf_.data();
  auto end = ptr + c_args_remainder_buf_.size();
  auto advance_ptr = [&ptr] {
    while (*ptr++ != '\0')
      ; // nop
  };
  for (; ptr != end; advance_ptr())
    c_args_remainder_.emplace_back(ptr);
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

error actor_system_config::parse(string_list args, std::istream& config) {
  // Contents of the config file override hard-coded defaults.
  if (config.good()) {
    if (auto err = parse_config(config, custom_options_, content))
      return err;
  } else {
    // Not finding an explicitly defined config file is an error.
    if (auto fname = get_if<std::string>(&content, "config-file"))
      return make_error(sec::cannot_open_file, *fname);
  }
  // CLI options override the content of the config file.
  using std::make_move_iterator;
  auto res = custom_options_.parse(content, args);
  if (res.second != args.end()) {
    if (res.first != pec::success && starts_with(*res.second, "-")) {
      return make_error(res.first, *res.second);
    } else {
      args.erase(args.begin(), res.second);
      set_remainder(std::move(args));
    }
  } else {
    cli_helptext_printed = get_or(content, "help", false)
                           || get_or(content, "long-help", false);
    set_remainder(string_list{});
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
  return none;
}

error actor_system_config::parse(string_list args,
                                 const char* config_file_cstr) {
  // Override default config file name if set by user.
  if (config_file_cstr != nullptr)
    config_file_path = config_file_cstr;
  // CLI arguments always win.
  if (auto err = extract_config_file_path(args))
    return err;
  if (config_file_path == "$DEFAULT") {
    std::ifstream conf{"caf-application.conf"};
    return parse(std::move(args), conf);
  }
  std::ifstream conf{config_file_path};
  return parse(std::move(args), conf);
}

actor_system_config& actor_system_config::add_actor_factory(std::string name,
                                                            actor_factory fun) {
  actor_factories.emplace(std::move(name), std::move(fun));
  return *this;
}

actor_system_config& actor_system_config::set_impl(string_view name,
                                                   config_value value) {
  auto opt = custom_options_.qualified_name_lookup(name);
  if (opt == nullptr) {
    std::cerr << "*** failed to set config parameter " << name
              << ": invalid name" << std::endl;
  } else if (auto err = opt->sync(value)) {
    std::cerr << "*** failed to set config parameter " << name << ": "
              << to_string(err) << std::endl;
  } else {
    auto category = opt->category();
    if (category == "global")
      content[opt->long_name()] = std::move(value);
    else
      put(content, name, std::move(value));
  }
  return *this;
}

std::string actor_system_config::render(const error& x) {
  return to_string(x);
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
  detail::config_consumer consumer{opts, result};
  parser_state<config_iter, config_sentinel> res{config_iter{&source}};
  detail::parser::read_config(res, consumer);
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
    auto str = std::move(*i);
    args.erase(i);
    return make_error(pec::missing_argument, std::move(str));
  }
  config_value val{path};
  if (auto err = ptr->sync(val); !err) {
    put(content, "config-file", std::move(val));
    return none;
  } else {
    return err;
  }
}

const settings& content(const actor_system_config& cfg) {
  return cfg.content;
}

} // namespace caf
