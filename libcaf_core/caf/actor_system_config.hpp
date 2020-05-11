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

#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <typeindex>
#include <unordered_map>

#include "caf/actor_factory.hpp"
#include "caf/actor_profiler.hpp"
#include "caf/config_option.hpp"
#include "caf/config_option_adder.hpp"
#include "caf/config_option_set.hpp"
#include "caf/config_value.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/safe_equal.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/dictionary.hpp"
#include "caf/fwd.hpp"
#include "caf/is_typed_actor.hpp"
#include "caf/named_actor_config.hpp"
#include "caf/settings.hpp"
#include "caf/stream.hpp"
#include "caf/thread_hook.hpp"

namespace caf {

/// Configures an `actor_system` on startup.
class CAF_CORE_EXPORT actor_system_config {
public:
  // -- member types -----------------------------------------------------------

  using hook_factory = std::function<io::hook*(actor_system&)>;

  using hook_factory_vector = std::vector<hook_factory>;

  using thread_hooks = std::vector<std::unique_ptr<thread_hook>>;

  template <class K, class V>
  using hash_map = std::unordered_map<K, V>;

  using module_factory = std::function<actor_system::module*(actor_system&)>;

  using module_factory_vector = std::vector<module_factory>;

  using actor_factory_map = hash_map<std::string, actor_factory>;

  using portable_name_map = hash_map<std::type_index, std::string>;

  using group_module_factory = std::function<group_module*()>;

  using group_module_factory_vector = std::vector<group_module_factory>;

  using config_map = dictionary<config_value::dictionary>;

  using string_list = std::vector<std::string>;

  using named_actor_config_map = hash_map<std::string, named_actor_config>;

  using opt_group = config_option_adder;

  // -- constructors, destructors, and assignment operators --------------------

  virtual ~actor_system_config();

  actor_system_config();

  actor_system_config(actor_system_config&&) = default;

  actor_system_config(const actor_system_config&) = delete;
  actor_system_config& operator=(const actor_system_config&) = delete;

  // -- properties -------------------------------------------------------------

  /// @private
  settings content;

  /// Extracts all parameters from the config, including entries with default
  /// values.
  virtual settings dump_content() const;

  /// Sets a config by using its INI name `config_name` to `config_value`.
  template <class T>
  actor_system_config& set(string_view name, T&& value) {
    return set_impl(name, config_value{std::forward<T>(value)});
  }

  // -- modifiers --------------------------------------------------------------

  /// Parses `args` as tuple of strings containing CLI options and `ini_stream`
  /// as INI formatted input stream.
  error parse(string_list args, std::istream& ini);

  /// Parses `args` as tuple of strings containing CLI options and tries to
  /// open `ini_file_cstr` as INI formatted config file. The parsers tries to
  /// open `caf-application.ini` if `ini_file_cstr` is `nullptr`.
  error parse(string_list args, const char* ini_file_cstr = nullptr);

  /// Parses the CLI options `{argc, argv}` and `ini_stream` as INI formatted
  /// input stream.
  error parse(int argc, char** argv, std::istream& ini);

  /// Parses the CLI options `{argc, argv}` and tries to open `ini_file_cstr`
  /// as INI formatted config file. The parsers tries to open
  /// `caf-application.ini` if `ini_file_cstr` is `nullptr`.
  error parse(int argc, char** argv, const char* ini_file_cstr = nullptr);

  /// Allows other nodes to spawn actors created by `fun`
  /// dynamically by using `name` as identifier.
  /// @experimental
  actor_system_config& add_actor_factory(std::string name, actor_factory fun);

  /// Allows other nodes to spawn actors of type `T`
  /// dynamically by using `name` as identifier.
  /// @experimental
  template <class T, class... Ts>
  actor_system_config& add_actor_type(std::string name) {
    using handle = typename infer_handle_from_class<T>::type;
    static_assert(has_type_id_v<handle>);
    return add_actor_factory(std::move(name), make_actor_factory<T, Ts...>());
  }

  /// Allows other nodes to spawn actors implemented by function `f`
  /// dynamically by using `name` as identifier.
  /// @experimental
  template <class F>
  actor_system_config& add_actor_type(std::string name, F f) {
    using handle = typename infer_handle_from_fun<F>::type;
    static_assert(has_type_id_v<handle>);
    return add_actor_factory(std::move(name), make_actor_factory(std::move(f)));
  }

  /// Loads module `T` with optional template parameters `Ts...`.
  template <class T, class... Ts>
  actor_system_config& load() {
    module_factories.push_back([](actor_system& sys) -> actor_system::module* {
      return T::make(sys, detail::type_list<Ts...>{});
    });
    return *this;
  }

  /// Adds a factory for a new hook type to the middleman (if loaded).
  template <class Factory>
  actor_system_config& add_hook_factory(Factory f) {
    hook_factories.push_back(f);
    return *this;
  }

  /// Adds a hook type to the middleman (if loaded).
  template <class Hook>
  actor_system_config& add_hook_type() {
    return add_hook_factory(
      [](actor_system& sys) -> Hook* { return new Hook(sys); });
  }

  /// Adds a hook type to the scheduler.
  template <class Hook, class... Ts>
  actor_system_config& add_thread_hook(Ts&&... ts) {
    std::unique_ptr<thread_hook> hook{new Hook(std::forward<Ts>(ts)...)};
    thread_hooks_.emplace_back(std::move(hook));
    return *this;
  }

  // -- parser and CLI state ---------------------------------------------------

  /// Stores whether the help text was printed. If set to `true`, the
  /// application should not use this config to initialize an `actor_system`
  /// and instead return from `main` immediately.
  bool cli_helptext_printed;

  /// Stores CLI arguments that were not consumed by CAF.
  string_list remainder;

  // -- caf-run parameters -----------------------------------------------------

  /// Stores whether this node was started in slave mode.
  bool slave_mode;

  /// Name of this node when started in slave mode.
  std::string slave_name;

  /// Credentials for connecting to the bootstrap node.
  std::string bootstrap_node;

  // -- stream parameters ------------------------------------------------------

  /// @private
  timespan stream_desired_batch_complexity;

  /// @private
  timespan stream_max_batch_delay;

  /// @private
  timespan stream_credit_round_interval;

  /// @private
  timespan stream_tick_duration() const noexcept;

  // -- OpenSSL parameters -----------------------------------------------------

  std::string openssl_certificate;
  std::string openssl_key;
  std::string openssl_passphrase;
  std::string openssl_capath;
  std::string openssl_cafile;

  // -- factories --------------------------------------------------------------

  actor_factory_map actor_factories;
  module_factory_vector module_factories;
  hook_factory_vector hook_factories;
  group_module_factory_vector group_module_factories;

  // -- hooks ------------------------------------------------------------------

  thread_hooks thread_hooks_;

  /// Provides system-wide callbacks for several actor operations.
  /// @experimental
  /// @note Has no effect unless building CAF with CAF_ENABLE_ACTOR_PROFILER.
  actor_profiler* profiler = nullptr;

  /// Enables CAF to deserialize application-specific tracing information.
  /// @experimental
  /// @note Has no effect unless building CAF with CAF_ENABLE_ACTOR_PROFILER.
  tracing_data_factory* tracing_context = nullptr;

  // -- parsing parameters -----------------------------------------------------

  /// Configures the file path for the INI file, `caf-application.ini` per
  /// default.
  std::string config_file_path;

  // -- utility for caf-run ----------------------------------------------------

  // Config parameter for individual actor types.
  named_actor_config_map named_actor_configs;

  int (*slave_mode_fun)(actor_system&, const actor_system_config&);

  // -- default error rendering functions --------------------------------------

  [[deprecated("please use to_string() on the error")]] static std::string
  render(const error& err);

  // -- config file parsing ----------------------------------------------------

  /// Tries to open `filename` and parses its content as CAF config file.
  /// @param filename Relative or absolute path to the config file.
  /// @returns A ::settings dictionary with the parsed content of `filename` on
  ///          success, an ::error otherwise.
  static expected<settings> parse_config_file(const char* filename);

  /// Tries to open `filename` and parses its content as CAF config file. Also
  /// type-checks user-defined parameters in `opts`.
  /// @param filename Relative or absolute path to the config file.
  /// @param opts User-defined config options for type checking.
  /// @returns A ::settings dictionary with the parsed content of `filename` on
  ///          success, an ::error otherwise.
  static expected<settings>
  parse_config_file(const char* filename, const config_option_set& opts);

  /// Tries to open `filename`, parses its content as CAF config file and
  /// stores all entries in `result` (overrides conflicting entries). Also
  /// type-checks user-defined parameters in `opts`.
  /// @param filename Relative or absolute path to the config file.
  /// @param opts User-defined config options for type checking.
  /// @param result Storage for parsed entries. Note that `result` will contain
  ///               partial results if this function returns an error.
  /// @returns A default-constructed ::error on success, the error code of the
  ///          parser otherwise.
  static error
  parse_config_file(const char* filename, const config_option_set& opts,
                    settings& result);

  /// Parses the content of `source` using CAF's config format.
  /// @param source Character sequence in CAF's config format.
  /// @returns A ::settings dictionary with the parsed content of `source` on
  ///          success, an ::error otherwise.
  static expected<settings> parse_config(std::istream& source);

  /// Parses the content of `source` using CAF's config format. Also
  /// type-checks user-defined parameters in `opts`.
  /// @param source Character sequence in CAF's config format.
  /// @param opts User-defined config options for type checking.
  /// @returns A ::settings dictionary with the parsed content of `source` on
  ///          success, an ::error otherwise.
  static expected<settings>
  parse_config(std::istream& source, const config_option_set& opts);

  /// Parses the content of `source` using CAF's config format and stores all
  /// entries in `result` (overrides conflicting entries). Also type-checks
  /// user-defined parameters in `opts`.
  /// @param source Character sequence in CAF's config format.
  /// @param opts User-defined config options for type checking.
  /// @param result Storage for parsed entries. Note that `result` will contain
  ///               partial results if this function returns an error.
  /// @returns A default-constructed ::error on success, the error code of the
  ///          parser otherwise.
  static error parse_config(std::istream& source, const config_option_set& opts,
                            settings& result);

protected:
  config_option_set custom_options_;

private:
  actor_system_config& set_impl(string_view name, config_value value);

  error extract_config_file_path(string_list& args);

  /// Adjusts the content of the configuration, e.g., for ensuring backwards
  /// compatibility with older options.
  error adjust_content();
};

/// Returns all user-provided configuration parameters.
CAF_CORE_EXPORT const settings& content(const actor_system_config& cfg);

/// Tries to retrieve the value associated to `name` from `cfg`.
/// @relates actor_system_config
template <class T>
auto get_if(const actor_system_config* cfg, string_view name) {
  return get_if<T>(&content(*cfg), name);
}

/// Retrieves the value associated to `name` from `cfg`.
/// @relates actor_system_config
template <class T>
T get(const actor_system_config& cfg, string_view name) {
  return get<T>(content(cfg), name);
}

/// Retrieves the value associated to `name` from `cfg` or returns
/// `default_value`.
/// @relates actor_system_config
template <class T, class = typename std::enable_if<
                     !std::is_pointer<T>::value
                     && !std::is_convertible<T, string_view>::value>::type>
T get_or(const actor_system_config& cfg, string_view name, T default_value) {
  return get_or(content(cfg), name, std::move(default_value));
}

/// Retrieves the value associated to `name` from `cfg` or returns
/// `default_value`.
/// @relates actor_system_config
inline std::string get_or(const actor_system_config& cfg, string_view name,
                          string_view default_value) {
  return get_or(content(cfg), name, default_value);
}

/// Returns whether `xs` associates a value of type `T` to `name`.
/// @relates actor_system_config
template <class T>
bool holds_alternative(const actor_system_config& cfg, string_view name) {
  return holds_alternative<T>(content(cfg), name);
}

} // namespace caf
