// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/actor_factory.hpp"
#include "caf/callback.hpp"
#include "caf/config_option.hpp"
#include "caf/config_option_adder.hpp"
#include "caf/config_option_set.hpp"
#include "caf/config_value.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/dictionary.hpp"
#include "caf/fwd.hpp"
#include "caf/settings.hpp"
#include "caf/thread_hook.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <type_traits>

namespace caf {

/// Configures an `actor_system` on startup.
class CAF_CORE_EXPORT actor_system_config {
public:
  // -- friends ----------------------------------------------------------------

  friend class actor_system;

  friend class detail::actor_system_config_access;

  // -- member types -----------------------------------------------------------

  using opt_group = config_option_adder;

  // -- constructors, destructors, and assignment operators --------------------

  actor_system_config();

  actor_system_config(actor_system_config&&) = default;

  actor_system_config(const actor_system_config&) = delete;

  actor_system_config& operator=(const actor_system_config&) = delete;

  virtual ~actor_system_config();

  // -- properties -------------------------------------------------------------

  /// Stores user-defined configuration parameters.
  settings content;

  /// Extracts all parameters from the config, including entries with default
  /// values.
  virtual settings dump_content() const;

  /// Sets a config by using its name `config_name` to `config_value`.
  template <class T>
  actor_system_config& set(std::string_view name, T&& value) {
    return set_impl(name, config_value{std::forward<T>(value)});
  }

  // -- modifiers --------------------------------------------------------------

  /// Parses `args` as tuple of strings containing CLI options and `config` as
  /// configuration file.
  error parse(std::vector<std::string> args, std::istream& config);

  /// Parses `args` as CLI options and tries to locate a config file via
  /// `config_file_path` and `config_file_path_alternative` unless the user
  /// provides a config file path on the command line.
  error parse(std::vector<std::string> args);

  /// Parses the CLI options `{argc, argv}` and `config` as configuration file.
  error parse(int argc, char** argv, std::istream& config);

  /// Parses the CLI options `{argc, argv}` and tries to locate a config file
  /// via `config_file_path` and `config_file_path_alternative` unless the user
  /// provides a config file path on the command line.
  error parse(int argc, char** argv);

  /// Allows other nodes to spawn actors created by `fun`
  /// dynamically by using `name` as identifier.
  actor_system_config& add_actor_factory(std::string name, actor_factory fun);

  /// Allows other nodes to spawn actors of type `T`
  /// dynamically by using `name` as identifier.
  template <class T, class... Ts>
  actor_system_config& add_actor_type(std::string name) {
    using handle = infer_handle_from_class_t<T>;
    static_assert(detail::is_complete<type_id<handle>>);
    return add_actor_factory(std::move(name), make_actor_factory<T, Ts...>());
  }

  /// Allows other nodes to spawn actors implemented as an `actor_from_state<T>`
  /// dynamically by using `name` as identifier.
  /// @param t conveys the state constructor signature as a type list.
  /// @param ts type lists conveying alternative constructor signatures.
  template <class F, class T, class... Ts>
  actor_system_config& add_actor_type(std::string name, F f, T t, Ts... ts) {
    return add_actor_factory(std::move(name), make_actor_factory(f, t, ts...));
  }

  /// Allows other nodes to spawn actors implemented by function `f`
  /// dynamically by using `name` as identifier.
  template <class F>
  actor_system_config& add_actor_type(std::string name, F f) {
    if constexpr (detail::has_handle_type_alias_v<F>) {
      // F represents an actor_from_state spawnable wrapper.
      return add_actor_factory(std::move(name),
                               make_actor_factory(f, type_list<>{}));
    } else {
      // F represents a function based actor callable.
      using handle = infer_handle_from_fun_t<F>;
      static_assert(detail::is_complete<type_id<handle>>);
      return add_actor_factory(std::move(name),
                               make_actor_factory(std::move(f)));
    }
  }

  /// Loads module `T`.
  template <class T>
  actor_system_config& load(version::abi_token token = make_abi_token()) {
    T::check_abi_compatibility(token);
    T::add_module_options(*this);
    add_module_factory([](actor_system& sys) -> actor_system_module* { //
      return T::make(sys);
    });
    return *this;
  }

  /// Adds a hook type to the scheduler.
  template <class Hook, class... Ts>
  actor_system_config& add_thread_hook(Ts&&... ts) {
    add_thread_hook(std::make_unique<Hook>(std::forward<Ts>(ts)...));
    return *this;
  }

  // -- parser and CLI state ---------------------------------------------------

  /// Returns whether the help text was printed. If this function return `true`,
  /// the application should not use this config object to initialize an
  /// `actor_system` and instead return from `main` immediately.
  bool helptext_printed() const noexcept;

  /// Stores the content of `argv[0]` from the arguments passed to `parse`.
  const std::string& program_name() const noexcept;

  /// Stores CLI arguments that were not consumed by CAF.
  span<const std::string> remainder() const noexcept;

  /// Returns the remainder including the program name (`argv[0]`) suitable for
  /// passing the returned pair of arguments to C-style functions that usually
  /// accept `(argc, argv)` input as passed to `main`. This function creates the
  /// necessary buffers lazily on first call.
  /// @note The returned pointer remains valid only as long as the
  ///       `actor_system_config` object exists.
  std::pair<int, char**> c_args_remainder() const noexcept;

  // -- config file parsing ----------------------------------------------------

  /// Sets the default path of the configuration file.
  void config_file_path(std::string path);

  /// Sets the default paths of the configuration file.
  void config_file_paths(std::vector<std::string> paths);

  /// Returns the default paths of the configuration file.
  span<const std::string> config_file_paths();

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
  static expected<settings> parse_config_file(const char* filename,
                                              const config_option_set& opts);

  /// Tries to open `filename`, parses its content as CAF config file and
  /// stores all entries in `result` (overrides conflicting entries). Also
  /// type-checks user-defined parameters in `opts`.
  /// @param filename Relative or absolute path to the config file.
  /// @param opts User-defined config options for type checking.
  /// @param result Storage for parsed entries. Note that `result` will contain
  ///               partial results if this function returns an error.
  /// @returns A default-constructed ::error on success, the error code of the
  ///          parser otherwise.
  static error parse_config_file(const char* filename,
                                 const config_option_set& opts,
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
  static expected<settings> parse_config(std::istream& source,
                                         const config_option_set& opts);

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

  /// @private
  config_option_set& custom_options() {
    return custom_options_;
  }

  /// @private
  void print_content() const;

protected:
  config_option_set custom_options_;

private:
  // -- module factories -------------------------------------------------------

  using module_factory_fn = actor_system_module* (*) (actor_system&);

  void add_module_factory(module_factory_fn new_factory);

  span<module_factory_fn> module_factories();

  // -- actor factories --------------------------------------------------------

  actor_factory* get_actor_factory(std::string_view name);

  // -- thread hooks -----------------------------------------------------------

  void add_thread_hook(std::unique_ptr<thread_hook> ptr);

  span<std::unique_ptr<thread_hook>> thread_hooks();

  // -- mailbox factory --------------------------------------------------------

  void mailbox_factory(std::unique_ptr<detail::mailbox_factory> factory);

  detail::mailbox_factory* mailbox_factory();

  // -- internal bookkeeping ---------------------------------------------------

  void set_remainder(std::vector<std::string> args);

  // -- utility functions ------------------------------------------------------

  actor_system_config& set_impl(std::string_view name, config_value value);

  std::pair<error, std::string>
  extract_config_file_path(std::vector<std::string>& args);

  // -- member variables -------------------------------------------------------

  struct fields;

  fields* fields_;
};

/// Returns all user-provided configuration parameters.
CAF_CORE_EXPORT const settings& content(const actor_system_config& cfg);

/// Returns whether `xs` associates a value of type `T` to `name`.
/// @relates actor_system_config
template <class T>
bool holds_alternative(const actor_system_config& cfg, std::string_view name) {
  return holds_alternative<T>(content(cfg), name);
}

/// Tries to retrieve the value associated to `name` from `cfg`.
/// @relates actor_system_config
template <class T>
auto get_if(const actor_system_config* cfg, std::string_view name) {
  return get_if<T>(&content(*cfg), name);
}

/// Retrieves the value associated to `name` from `cfg`.
/// @relates actor_system_config
template <class T>
T get(const actor_system_config& cfg, std::string_view name) {
  return get<T>(content(cfg), name);
}

/// Retrieves the value associated to `name` from `cfg` or returns `fallback`.
/// @relates actor_system_config
template <class To = get_or_auto_deduce, class Fallback>
auto get_or(const actor_system_config& cfg, std::string_view name,
            Fallback&& fallback) {
  return get_or<To>(content(cfg), name, std::forward<Fallback>(fallback));
}

/// Tries to retrieve the value associated to `name` from `cfg` as an instance
/// of type `T`.
/// @relates actor_system_config
template <class T>
expected<T> get_as(const actor_system_config& cfg, std::string_view name) {
  return get_as<T>(content(cfg), name);
}

} // namespace caf
