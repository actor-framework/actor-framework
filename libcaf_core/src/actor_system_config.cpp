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
  add_message_type_impl<settings>("settings");
  add_message_type_impl<config_value::list>("std::vector<@config_value>");
  add_message_type_impl<config_value::dictionary>("dictionary<@config_value>");
  // (1) hard-coded defaults
  stream_desired_batch_complexity = defaults::stream::desired_batch_complexity;
  stream_max_batch_delay = defaults::stream::max_batch_delay;
  stream_credit_round_interval = defaults::stream::credit_round_interval;
  // fill our options vector for creating INI and CLI parsers
  using std::string;
  opt_group{custom_options_, "global"}
    .add<bool>("help,h?", "print help text to STDERR and exit")
    .add<bool>("long-help", "print long help text to STDERR and exit")
    .add<bool>("dump-config", "print configuration to STDERR and exit");
  opt_group{custom_options_, "stream"}
    .add<timespan>(stream_desired_batch_complexity, "desired-batch-complexity",
                   "processing time per batch")
    .add<timespan>(stream_max_batch_delay, "max-batch-delay",
                   "maximum delay for partial batches")
    .add<timespan>(stream_credit_round_interval, "credit-round-interval",
                   "time between emitting credit");
  opt_group{custom_options_, "scheduler"}
    .add<atom_value>("policy", "'stealing' (default) or 'sharing'")
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
    .add<atom_value>("verbosity", "default verbosity for file and console")
    .add<string>("file-name", "filesystem path of the log file")
    .add<string>("file-format", "line format for individual log file entires")
    .add<atom_value>("file-verbosity", "file output verbosity")
    .add<atom_value>("console", "std::clog output: none, colored, or uncolored")
    .add<string>("console-format", "line format for printed log entires")
    .add<atom_value>("console-verbosity", "console output verbosity")
    .add<std::vector<atom_value>>("component-blacklist",
                                  "excluded components for logging")
    .add<bool>("inline-output", "disable logger thread (for testing only!)");
  opt_group{custom_options_, "middleman"}
    .add<atom_value>("network-backend",
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
    .add<size_t>("cached-udp-buffers",
                 "maximum for cached UDP send buffers (default: 10)")
    .add<size_t>("max-pending-messages",
                 "maximum for reordering of UDP receive buffers (default: 10)")
    .add<bool>("disable-tcp", "disables communication via TCP")
    .add<bool>("enable-udp", "enable communication via UDP");
  opt_group(custom_options_, "opencl")
    .add<std::vector<size_t>>("device-ids", "whitelist for OpenCL devices");
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

struct ini_sentinel { };

bool operator!=(ini_iter iter, ini_sentinel) {
  return !iter.ini->fail();
}

} // namespace <anonymous>

error actor_system_config::parse(string_list args, std::istream& ini) {
  // Content of the INI file overrides hard-coded defaults.
  if (ini.good()) {
    detail::ini_consumer consumer{custom_options_, content};
    detail::parser::state<ini_iter, ini_sentinel> res;
    res.i = ini_iter{&ini};
    detail::parser::read_ini(res, consumer);
    if (res.i != res.e)
      return make_error(res.code, res.line, res.column);
  }
  // CLI options override the content of the INI file.
  using std::make_move_iterator;
  auto res = custom_options_.parse(content, args);
  if (res.second != args.end()) {
    if (res.first != pec::success) {
      return make_error(res.first, *res.second);
      std::cerr << "error: at argument \"" << *res.second
                << "\": " << to_string(res.first) << std::endl;
      cli_helptext_printed = true;
    }
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
  // Generate INI dump if needed.
  if (!cli_helptext_printed && get_or(content, "dump-config", false)) {
    for (auto& category : content) {
      if (auto dict = get_if<config_value::dictionary>(&category.second)) {
        std::cout << '[' << category.first << "]\n";
        for (auto& kvp : *dict)
          if (kvp.first != "dump-config")
            std::cout << kvp.first << '=' << to_string(kvp.second) << '\n';
      }
    }
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
  extract_config_file_path(args);
  std::ifstream ini{config_file_path};
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
std::string actor_system_config::render(const error& err) {
  std::string msg;
  switch (static_cast<uint64_t>(err.category())) {
    case atom_uint("system"):
      return render_sec(err.code(), err.category(), err.context());
    case atom_uint("exit"):
      return render_exit_reason(err.code(), err.category(), err.context());
    case atom_uint("parser"):
      return render_pec(err.code(), err.category(), err.context());
  }
  return "unknown-error";
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

std::string actor_system_config::render_pec(uint8_t x, atom_value,
                                            const message& xs) {
  auto tmp = static_cast<pec>(x);
  return deep_to_string(meta::type_name("parser_error"), tmp,
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
  auto arg_begin = i->begin() + strlen(needle);
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
