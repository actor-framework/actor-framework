# Changelog

All notable changes to this project will be documented in this file. The format
is based on [Keep a Changelog](https://keepachangelog.com).

## [0.18.0] - Unreleased

### Added

- The new `fan_out_request` function streamlines fan-out/fan-in work flows (see
  the new example in `examples/message_passing/fan_out_request.cpp` as well as
  the new manual entry). The policy-based design further allows us to support
  more use cases in the future (#932, #964).
- We introduced the lightweight template class `error_code` as an alternative to
  the generic but more heavyweight class `error`. The new error code abstraction
  simply wraps an enumeration type without allowing users to add additional
  context such as error messages. However, whenever such information is
  unneeded, the new class is much more efficient than using `error`.
- Tracing messages in distributed systems is a common practice for monitoring
  and debugging message-based systems. The new `tracing_data` abstraction in CAF
  enables users to augment messages between actors with arbitrary meta data.
  This is an experimental API that requires building CAF with the CMake option
  `CAF_ENABLE_ACTOR_PROFILER` (#981).
- Add compact `from..to..step` list notation in configuration files. For
  example, `[1..3]` expands to `[1, 2, 3]` and `[4..-4..-2]` expands to
  `[4, 2, 0, -2, -4]` (#999).
- Allow config keys to start with numbers (#1014).
- The `fan_out_request` function got an additional policy for picking just the
  fist result: `select_any` (#1012).
- Run-time type information in CAF now uses 16-bit type IDs. Users can assign
  this ID by specializing `type_id` manually (not recommended) or use the new
  API for automatically assigning ascending IDs inside `CAF_BEGIN_TYPE_ID_BLOCK`
  and `CAF_END_TYPE_ID_BLOCK` code blocks.
- The new typed view types `typed_message_view` and `const_typed_message_view`
  make working with `message` easier by providing a `std::tuple`-like interface
  (#1034).
- The class `exit_msg` finally got its missing `operator==` (#1039).
- The class `node_id` received an overload for `parse` to allow users to convert
  the output of `to_string` back to the original ID (#1058).
- Actors can now `monitor` and `demonitor` CAF nodes (#1042). Monitoring a CAF
  node causes the actor system to send a `node_down_msg` to the observer when
  losing connection to the monitored node.

### Changed

- CAF now requires C++17 to build.
- On UNIX, CAF now uses *visibility hidden* by default. All API functions and
  types that form the ABI are explicitly exported using module-specific macros.
  On Windows, this change finally enables building native DLL files.
- We switched our coding style to the C++17 nested namespace syntax.
- CAF used to generate the same node ID when running on the same machine and
  only differentiates actors systems by their process ID. When running CAF
  instances in a container, this process ID is most likely the same for each
  run. This means two containers can produce the same node ID and thus
  equivalent actor IDs. In order to make it easier to use CAF in a containerized
  environment, we now generate unique (random) node IDs (#970).
- We did a complete redesign of all things serialization. The central class
  `data_processor` got removed. The two classes for binary serialization no
  longer extend the generic interfaces `serializer` and `deserializer` in order
  to avoid the overhead of dynamic dispatching as well as the runtime cost of
  `error` return values. This set of changes leads so some code duplication,
  because many CAF types now accept a generic `(de)serializer` as well as a
  `binary_(de)serializer` but significantly boosts performance in the hot code
  paths of CAF (#975).
- With C++17, we no longer support compilers without support for `thread_local`.
  Consequently, we removed all workarounds and switched to the C++ keyword
  (#996).
- Our manual now uses `reStructuredText` instead of `LaTeX`. We hope this makes
  extending the manual easier and lowers the barrier to entry for new
  contributors.

### Removed

- A vendor-neutral API for GPGPU programming sure sounds great. Unfortunately,
  OpenCL did not catch on in the way we had hoped. At this point, we can call
  OpenCL dead and gone. There is only legacy support available and recent
  versions of the standard were never implemented in the first place.
  Consequently, we've dropped the `opencl` module.
- The old `duration` type is now superseded by `timespan` (#994).

### Fixed

- Fix uninstall target when building CAF as CMake subdirectory.
- Using `inline_all_enqueues` in deterministic unit tests could result in
  deadlocks when calling blocking functions in message handlers. This function
  now behaves as expected (#1016).
- Exceptions while handling requests now trigger error messages (#1055).
- The member function `demonitor` falsely refused typed actor handles. Actors
  could monitor typed actors but not demonitoring it again. This member function
  is now a template that accepts any actor handle in the same way `monitor`
  already did.

## [0.17.5] - Unreleased

### Changed

- Our manual now uses `reStructuredText` instead of `LaTeX` (backport from
  [0.18.0]).

### Fixed

- Fix handling of OS-specific threading dependency in CMake.
- Fix uninstall target when building CAF as CMake subdirectory (backport from
  [0.18.0]).
- Fix potential deadlock with `inline_all_enqueues` (backport from [0.18.0]).
- Exceptions while handling requests now trigger error messages (backport from
  [0.18.0]).

## [0.17.4] - 2019-02-08

### Added

- The class `exit_msg` finally got its missing `operator==` (#1039, backport
  from [0.18.0]).

### Changed

- Make sure actors that receive stream input shut down properly (#1019).
- Improve `to_string` output for `caf::error` (#1021).
- Properly report errors to users while connecting two CAF nodes (#1023).
- Simplify crosscompilation: remove build dependency on code generators (#1026).
- Leave CXX settings to the (CMake) parent when building as subdirectory (#1032).
- Build without documentation in subdirectory mode (#1037).
- Allow parents to set `OPENSSL_INCLUDE_DIR` in subdirectory mode (#1037).
- Add `-pthread` flag on UNIX when looking for `libc++` support (#1038).
- Avoid producing unexpected log files (#1024).

### Fixed

- Accept numbers as keys in the config syntax (#1014).
- Fix undesired function hiding in `fused_downstream_manager` (#1020).
- Fix behavior of `inline_all_enqueues` in the testing DSL (#1016).
- Fix path recognition in the URI parser, e.g., `file:///` is now valid (#1013).

## [0.17.3] - 2019-11-11

### Added

- Add support for OpenBSD (#955).
- Provide uniform access to actor properties (#958).
- Add missing `to_string(pec)` (#940).

### Fixed

- Fix bug in stream managers that caused finalizers to get called twice (#937).
- Fix verbosity level with disabled console output (#953).
- Fix excessive buffering in stream stages (#952).

## [0.17.2] - 2019-10-20

### Added

- Add `scheduled_send` for delayed sends with absolute timeout (#901).
- Allow actors based on composable behaviors to use the streaming API (#902).
- Support arbitrary list and map types in `config_value` (#925).
- Allow users to extend the `config_value` API (#929, #938).

### Changed

- Reduce stack usage of serializers (#912).
- Use default installation directories on GNU/Linux (#917).

### Fixed

- Fix memory leak when deserializing `node_id` (#905).
- Fix composition of statically typed actors using streams (#908).
- Fix several warnings on GCC and Clang (#915).
- Fix `holds_alternative` and `get_if` for `settings` (#920).
- Fix silent dropping of errors in response handlers (#935).
- Fix stall in `remote_group` on error (#933).

## [0.17.1] - 2019-08-31

### Added

- Support nesting of group names in .ini files (#870).
- Support all alphanumeric characters in config group names (#869).

### Changed

- Improve CMake setup when building CAF as subfolder (#866).
- Properly set CLI remainder (#871).

### Fixed

- Fix endless loop in config parser (#894).
- Fix debug build with Clang 7 on Linux (#861).
- Fix type-specific parsing of config options (#814).
- Fix potential deadlock in proxy registry (#880).
- Fix output of --dump-config (#876).
- Fix potential segfault when using streams with trace logging enabled (#878).
- Fix handling of containers with user-defined types (#867).
- Fix `defaulted_function_deleted` warning on Clang (#859).

## [0.17.0] - 2019-07-27

### Added

- Add marker to make categories optional on the CLI. Categories are great at
  organizing program options. However, on the CLI they get in the way quickly.
  This change allows developers to prefix category names with `?` to make it
  optional on the CLI.
- Add conversion from `nullptr` to intrusive and COW pointer types.
- Support move-only behavior functions.
- Allow users to omit `global` in config files.
- Allow IPO on GCC/Clang.

### Changed

- Parallelize deserialization of messages received over the network (#821).
  Moving the deserialization out of the I/O loop significantly increases
  performance. In our benchmark, CAF now handles up to twice as many messages
  per second.
- Relax ini syntax for maps by making `=` for defining maps and `,` for
  separating key-value pairs optional. For example, this change allows to
  rewrite an entry like this:
  ```ini
  logger = {
    console-verbosity='trace',
    console='colored'
  }
  ```
  to a slightly less noisy version such as this:
  ```ini
  logger {
    console-verbosity='trace'
    console='colored'
  }
  ```
- Allow apps to always use the `logger`, whether or not CAF was compiled with
  logging enabled.
- Streamline direct node-to-node communication and support multiple app
  identifiers.
- Reimplement `binary_serializer` and `binary_deserializer` without STL-style
  stream buffers for better performance.

### Fixed

- Fix performance of the thread-safe actor clock (#849). This clock type is used
  whenever sending requests, delayed messages, receive timeouts etc. With this
  change, CAF can handle about 10x more timeouts per second.
- Fix multicast address detection in `caf::ipv4_address.cpp` (#853).
- Fix disconnect issue / WSAGetLastError usage on Windows (#846).
- Fix `--config-file` option (#841).
- Fix parsing of CLI arguments for strings and atom values.

## [0.16.5] - 2019-11-11

### Added

- Support for OpenBSD.

## [0.16.4] - 2019-11-11

### Fixed

- Backport parser fixes from the CAF 0.17 series.
- Silence several compiler warnings on GCC and Clang.

## [0.16.3] - 2018-12-27

### Added

- The new class `cow_tuple` provides an `std::tuple`-like interface for a
  heap-allocated, copy-on-write tuple.
- Missing overloads for `dictionary`.
- The new `to_lowercase` function for atoms allows convenient conversion without
  having to convert between strings and atoms.

### Changed

- Printing timestamps now consistently uses ISO 8601 format.
- The logger now uses a bounded queue. This change in behavior will cause the
  application to slow down when logging faster than the logger can do I/O, but
  the queue can no longer grow indefinitely.
- Actors now always try to dequeue from the high-priority queue first.

### Fixed

- Solved linker errors related to `socket_guard` in some builds.
- Fix the logger output for class names.
- Deserializing into non-empty containers appended to the content instead of
  overriding it. The new implementation properly clears the container before
  filling it.
- The `split` function from the string algorithms header now works as the
  documentation states.
- Silence several compiler warnings on GCC and Clang.

## [0.16.2] - 2018-11-03

### Fixed

- The copy-on-write pointer used by `message` failed to release memory in some
  cases. The resulting memory leak is now fixed.

## [0.16.1] - 2018-10-31

### Added

- Adding additional flags for the compiler when using the `configure` script is
  now easier thanks to the `--extra-flags=` option.
- The actor clock now supports non-overriding timeouts.
- The new `intrusive_cow_ptr` is a smart pointer for copy-on-write access.

### Changed

- Improve `noexcept`-correctness of `variant`.
- CAF threads now have recognizable names in a debugger.
- The middleman now passes `CLOEXEC` on `socket`/`accept`/`pipe` calls.
- Users can now set the log verbosity for file and console output separately.

### Fixed

- A `dictionary` now properly treats C-strings as strings when using `emplace`.
- Eliminate a potential deadlock in the thread-safe actor clock.
- Added various missing includes and forward declarations.

## [0.16.0] - 2018-09-03

### Added

- As part of [CE-0002](https://git.io/Jvoo5), `config_value` received support
  for lists, durations and dictionaries. CAF now exposes the content of an actor
  system config as a dictionary of `config_value`. The free function `get_or`
  offers convenient access to configuration parameters with hard-coded defaults
  as fallback.
- The C++17-compatible `string_view` class enables us to make use of recent
  standard addition without having to wait until it becomes widely available.
- In preparation of plans for future convenience API, we've added `uri`
  according to [RFC 3986](https://tools.ietf.org/html/rfc3986) as well as
  `ipv6_address` and `ipv4_address`.
- A new, experimental streaming API. Please have a look at the new manual
  section for more details.

### Deprecated

- Going forward, the preferred way to access configuration parameters is using
  the new `get_or` API. Hence, these member variables are now deprecated in
  `actor_system_config`:
  + `scheduler_policy`
  + `scheduler_max_threads`
  + `scheduler_max_throughput`
  + `scheduler_enable_profiling`
  + `scheduler_profiling_ms_resolution`
  + `scheduler_profiling_output_file`
  + `work_stealing_aggressive_poll_attempts`
  + `work_stealing_aggressive_steal_interval`
  + `work_stealing_moderate_poll_attempts`
  + `work_stealing_moderate_steal_interval`
  + `work_stealing_moderate_sleep_duration_us`
  + `work_stealing_relaxed_steal_interval`
  + `work_stealing_relaxed_sleep_duration_us`
  + `logger_file_name`
  + `logger_file_format`
  + `logger_console`
  + `logger_console_format`
  + `logger_component_filter`
  + `logger_verbosity`
  + `logger_inline_output`
  + `middleman_network_backend`
  + `middleman_app_identifier`
  + `middleman_enable_automatic_connections`
  + `middleman_max_consecutive_reads`
  + `middleman_heartbeat_interval`
  + `middleman_detach_utility_actors`
  + `middleman_detach_multiplexer`
  + `middleman_cached_udp_buffers`
  + `middleman_max_pending_msgs`

### Removed

- The `boost::asio` was part of an initiative to contribute CAF as
  `boost::actor`. Since there was little interest by the Boost community, this
  backend now serves no purpose.

### Fixed

- Setting the log level to `quiet` now properly suppresses any log output.
- Configuring colored terminal output should now print colored output.

[0.18.0]: https://github.com/actor-framework/actor-framework/compare/0.17.3...master
[0.17.5]: https://github.com/actor-framework/actor-framework/compare/0.17.4...release/0.17
[0.17.4]: https://github.com/actor-framework/actor-framework/releases/0.17.4
[0.17.3]: https://github.com/actor-framework/actor-framework/releases/0.17.3
[0.17.2]: https://github.com/actor-framework/actor-framework/releases/0.17.2
[0.17.1]: https://github.com/actor-framework/actor-framework/releases/0.17.1
[0.17.0]: https://github.com/actor-framework/actor-framework/releases/0.17.0
[0.16.5]: https://github.com/actor-framework/actor-framework/releases/0.16.5
[0.16.4]: https://github.com/actor-framework/actor-framework/releases/0.16.4
[0.16.3]: https://github.com/actor-framework/actor-framework/releases/0.16.3
[0.16.2]: https://github.com/actor-framework/actor-framework/releases/0.16.2
[0.16.1]: https://github.com/actor-framework/actor-framework/releases/0.16.1
[0.16.0]: https://github.com/actor-framework/actor-framework/releases/0.16.0
