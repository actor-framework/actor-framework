# Changelog

All notable changes to this project will be documented in this file. The format
is based on [Keep a Changelog](https://keepachangelog.com).

## Unreleased

### Changed

- Add intermediary types for the `mail` API as `[[nodiscard]]` to make it easier
  to spot mistakes when chaining calls.
- The `merge` and `flat_map` operators now accept an optional unsigned integer
  parameter to configure the maximum number of concurrent subscriptions.
- If an actor terminates, it will now consistently send error messages with code
  `caf::sec::request_receiver_down` to all outstanding requests. Any request
  that arrives after an actor has closed its mailbox will receive the same error
  code. This change makes it easier to handle errors in a consistent way and to
  distinguish between requests that have been dropped and those that resulted in
  an error while processing the request (#2070).
- The URI parser in CAF now accepts URIs that use reserved characters such as
  `*`, `/` or `?` in the query string. This change follows the recommendation in
  the RFC 3986, which states that "query components are often used to carry
  identifying information in the form of key=value pairs and one frequently used
  value is a reference to another URI".
- CAF now respects CPU limits when running in a container to determine the
  thread pool size for the scheduler (#2061).

### Added

- New flow operators: `retry`, `combine_latest`, `debounce`, `throttle_first`
  and `on_error_resume_next`.
- New `with_userinfo` member function for URIs that allows setting the user-info
  sub-component without going through an URI builder.
- CAF now supports chunked encoding for HTTP clients (#2038).
- Added a missing configuration option to the HTTP client API that allows
  users to set the maximum size of the response.
- Add functions to the SSL API to enable Server Name Indication (SNI, #2084).
- Add `throttle_last` to observables: an alias for `sample`.

### Fixed

- Fix build error in `caf-net` when building with C++23 (#1919).
- Restructure some implementation details of `intrsuive_ptr` (no functional
  changes) to make it easier for `clang-tidy` to analyze the code. This fixes a
  false positive reported by `clang-tidy` in some use cases where `clang-tidy`
  would falsely report a use-after-free bug.
- Closing a WebSocket connection now properly sends a close frame to the client
  before closing the TCP connection (#1938).
- Fix a build error in the unit tests when building with C++20 and setting
  `CAF_USE_STD_FORMAT` to `ON` (#1963).
- The functions `run_scheduled`, `run_scheduled_weak`, `run_delayed` and
  `run_delayed_weak` now properly accept move-only callback types.
- Fix the conversion from invalid actor handles to raw pointers when using
  `actor_cast`. This resolves an issue with `send` that could lead to undefined
  behavior (#1972).
- Add missing export declaration for the `caf::net::prometheus` symbol (#2042).
- Boolean flags now accept arguments on the command line (#2048). For example,
  `--foo=true` is now equivalent to `--foo` and `--foo=false` will set the flag
  to `false`. Short options like `-f` only accept the argument when passing it
  without a space, e.g., `-ffalse`.
- The `buffer` and `interval` operators now properly check the time period
  parameter and produce an error if the period is zero or negative (#2030).
  Passing an invalid delay parameter to `to_stream` or `to_typed_stream`
  likewise produces a stream that immediately calls `on_error` on any client
  that tries to subscribe to it.
- Use `localtime_s` on all Windows platforms to fix a build error with
  MSYS/UCRT64 (#2059).
- Fix rendering of nested JSON lists (#2068).
- Add missing `pragma once` guards to multiple headers under `caf/net/ssl/`.
- Fix the behavior of `use_certificate_file_if` and `use_private_key_file_if`.
  Both functions did not leave the `caf::net::ssl::context` unchanged if one of
  the arguments was invalid but instead applied the invalid arguments to the
  context regardless, resulting in an OpenSSL error.
- Fix a bug in the HTTP parser that could cause the parser to try parsing the
  payload as a new request.
- Fix a startup issue when configuring Prometheus export on `caf.net` (#2060).
  This bug caused the Prometheus server to never start up unless starting at
  least one other asynchronous server or client using the `caf.net` API.
- Fix a bug in the URI parser that could crash the application when parsing an
  URI with a percent-encoded character at the end of the string (#2080).
- Fix parsing of HTTP request headers that do not use the absolute path syntax
  in the first line (#2074).
- Optimize templates for compile-time checks used by typed behaviors. This
  drastically reduces memory usage during compilation and avoids OOM errors when
  spawning typed actors with a large number of message handlers (#1970).
- BDD outlines now properly handle multiple `WHEN` blocks in a single
  scenario (#1776).
- Fix build issues on Clang 20 when building in C++20 mode (#2092).

## [1.0.2] - 2024-10-30

### Changed

- Tests, scenarios and outlines are now automatically put into the anonymous
  namespace to avoid name clashes with other tests. This removes the need for
  users to manually put their tests into an anonymous namespace.
- The `SUITE` macro of the unit test framework can now be used multiple times in
  a single compilation unit.

### Fixed

- When using the HTTP client API, the client now properly closes the connection
  after receiving a response even if the server would keep the connection open.
- When using the WebSocket client API, the socket could close prematurely when
  leaving `main` right after setting up the connection, even when starting an
  actor in `start`. This was due to CAF not holding onto a strong reference to
  the connection object (and thus the actor) at all times (#1918).
- When using log statements in unit tests, e.g. `log::test::debug`, the output
  will now be rendered by default even when not using the deterministic fixture.
- Registering a custom option of type `size_t` will no longer print
  `<dictionary>` as type hint in the `--help` output. Instead, CAF will print
  either `uint32_t` or `uint64_t`, depending on the platform.
- Fix handling of unicode escaping (e.g. `\u00E4`) in the JSON parser (#1937).

## [1.0.1] - 2024-07-23

### Fixed

- Fix a compiler error when using `spawn_client` on the I/O middleman (#1900).
- An unfortunate bug prevented the auto-detection of `std::format` when building
  CAF with C++20 or later. This bug has been fixed, alongside issues with the
  actual `std::format`-based implementation. However, since selecting a
  different backend for `println` and the log output generation breaks the ABI,
  we have decided to ship the `std::format`-based implementation only as opt-in
  at this point. Users can enable it by setting the CMake option
  `CAF_USE_STD_FORMAT`.
- Fix cleanup in the consumer adapter. This component connects actors to SPSC
  buffers. If the SPSC buffer was closed by the producer, the consumer adapter
  failed to properly dispose pending actions.
- Fix a `nullptr`-dereference in `scheduled_actor::run_actions` if a delayed
  action calls `self->quit()` (#1920).

### Changed

- When disposing a connection acceptor, CAF no longer generates a log event with
  severity `error`. Instead, it will log the event with severity `debug`.
- The `http::router` member function `shutdown` has been deprecated and renamed to
  `abort_and_shutdown` to better reflect its functionality.

## [1.0.0] - 2024-06-26

### Added

- The actor system now offers a `println` convenience function for printing to
  the console. When building CAF with C++20 enabled, the function uses
  `std::format` internally. Otherwise, it falls back to the compatibility layer
  with the same syntax. The function also recognizes `inspect` overloads for
  custom types. Any printing done by this function is thread-safe. The function
  is available on actors in order to allow `self->println("...")`.
- Actors have a new `monitor` member function that takes an actor handle and a
  callback. The callback will run when the monitored actor terminates (in the
  context of the monitoring actor). The function returns a `monitorable` object
  that can be used to cancel the monitoring. This mechanism replaces the old
  approach that relied on `down_msg` handlers in event-based actors (nothing
  changes for blocking actors).
- Event-based actors can now set an idle timeout via `set_idle_handler`. The
  timeout fires when the actor receives no messages for a specified duration.
  This new feature replaces the `after(...) >> ...` syntax and allows actors to
  specify what kind of reference CAF will hold to that actor while it is waiting
  for the timeout (strong or weak) and whether to trigger the timeout once or
  repeatedly.
- New flow operators: `buffer`, `sample`, `start_with`,
  `on_backpressure_buffer`, `on_error_return`, `on_error_return_item`, and
  `on_error_complete`.
- The unit test framework now offers the new utility (template) class
  `caf::test::approx` for approximate comparisons.

### Fixed

- Fix building CAF with shared libraries (DLLs) enabled on Windows (#1715).
- The `actor_from_state` utility now evaluates spawn options such as `linked`
  (#1771). Previously, passing this option to `actor_from_state` resulted in a
  compiler error.
- Sending a message to an already terminated actor from a `function_view` now
  properly reports an error (#1801).
- URIs now support support username and password in the user-info sub-component
  (#1814). Consequently, the `userinfo` field of the URI class now has two
  member variables: `name` and (an optional) `password`. Further, the `userinfo`
  field is now optional in order to differentiate between an empty user-info and
  no user-info at all.
- The parser for reading JSON and configuration files now properly handles
  Windows-style line endings (#1850).
- Calling `force_utc` on a `caf::chrono::datetime` object now properly applies
  the UTC offset. Previously, the function would shift the time into the wrong
  direction (#1860).
- Fix a regression in the work-stealing scheduler that prevented workers from
  stealing work from other workers in the relaxed polling state (#1866).
- Fix handling of integer or boolean values as keys as well as lists as values
  in dictionaries when using the `json_builder`.
- Calling `caf::chrono::datetime::to_local_time` will now properly interpret the
  stored time as local time if no UTC offset is present.

### Removed

- The obsolete CAF types `caf::string_view`, `caf::byte`, `caf::optional`,
  `caf::replies_to`, and `caf::flow::item_publisher`.
- The obsolete `operator*` for "combining" two actor handles.
- All `to_stream` and `to_typed_stream` member functions on actors (they are
  available on `caf::flow::observable` directly).
- The `group` API has been removed entirely.
- The experimental APIs for actor profiling and inserting tracing data have been
  removed. Neither API has a clear use case at the moment and since we have not
  received any feedback on either API, we have decided to remove them to
  simplify the code base.
- The `actor_system_config` no longer contains special member variables for the
  OpenSSL module. The module now uses the regular configuration system.
- The `caf-run` tool no longer ships with CAF. The tool has not been maintained
  for a long time, has never been thoroughly tested, and has no documentation.
- The `actor_system_config` no longer contains the `logger_factory` setter. We
  might reintroduce this feature in the future, but we think the new `logger`
  interface class is not yet stable enough to expose it to users and to allow
  custom logger implementations.


### Changed

- When using the caf-net module, users may enable Prometheus metric export by
  setting the configuration option `caf.net.prometheus-http`. The option has the
  following fields: `port`, `address`, `tls.key-file`, and `tls.cert-file`. When
  setting the TLS options, the server will use HTTPS instead of HTTP.
- Sending messages from cleanup code (e.g., the destructor of a state class) is
  now safe. Previously, doing so could cause undefined behavior by forming a
  strong reference to a destroyed actor.
- Actors will now always send an error message if an incoming message triggered
  an unhandled exception. Previously, CAF would only send an error message if
  the incoming message was a request (#1684).
- Stateful actors now provide a getter function `state()` instead of declaring a
  public `state` member variable. This change enables more flexibility in the
  implementation for future CAF versions.
- Passing a class to `spawn_inactive` is now optional and defaults to
  `event_based_actor`. The next major release will remove the class parameter
  altogether.
- Event-based actors can now handle types like `exit_msg` and `error` in their
  regular behavior.
- The `check_eq` and `require_eq` functions in the unit test framework now
  prohibit comparing floating-point numbers with `==`. Instead, users should use
  new `approx` utility class.
- The member functions `send`, `scheduled_send`, `delayed_send`, `request` and
  `delegate` are deprecated in favor of the new `mail` API.
- The free functions `anon_send`, `delayed_anon_send`, `scheduled_anon_send` are
  deprecated in favor of the new `anon_mail` API.

### Deprecated

- Using `spawn_inactive` now emits a deprecation warning when passing a class
  other than `event_based_actor`.
- The experimental `actor_pool` API has been deprecated. The API has not seen
  much use and is too cumbersome.
- The printing interface via `aout` has been replaced by the new `println`
  function. The old interface is now deprecated and will be removed in the next
  major release.
- Calling `monitor` with a single argument is now deprecated for event-based
  actors. Users should always provide a callback as the second argument. Note
  that nothing changes for blocking actors. They still call `monitor` and then
  receive a `down_msg` in their mailbox.
- The function `set_down_handler` is deprecated for the same reasons as
  `monitor`: please use the new `monitor` API with a callback instead.
- The spawn flag `monitored` is deprecated as well and users should call
  `monitor` directly instead.
- Constructing a behavior with `after(...) >> ...` has been deprecated in favor
  of the new `set_idle_handler` function. Note that blocking actors may still
  pass a timeout via `after(...)` to `receive` functions. The deprecation only
  targets event-based actors.
- The use case for `function_view` is covered by the new feature on blocking
  actors that allows calling `receive` with no arguments. Hence, `function_view`
  becomes obsolete and is deprecated.
- The `set_default_handler` member function on event-based actors is now
  deprecated. Instead, users should use a handler for `message` in their
  behavior as a catch-all. For skipping messages, CAF now includes a new
  `mail_cache` class that allows explicitly stashing messages for later
  processing.
- Special-purpose handlers for messages like `exit_msg` and `error` are now
  deprecated. Instead, users should handle these messages in their regular
  behavior.
- The legacy testing framework in `caf/test/unit_test.hpp` is now deprecated and
  the header (as well as headers that build on top it such as
  `caf/test/dsl.hpp`) will be removed in the next major release. Users should
  migrate to the new testing framework.

## [0.19.5] - 2024-01-08

### Added

- An `observable` that runs on an actor can now be converted to a `stream` or
  `typed_stream` directly by calling `to_stream` or `to_typed_stream` on it.
- New `caf::async` API to read text and binary files asynchronously in a
  separate thread. The contents of the files are consumed as flows (#1573).
- The class `caf::unordered_flat_map` now has the `contains` and
  `insert_or_assign` member functions.
- CAF now supports setting custom configuration options via environment
  variables. The new priority order is: CLI arguments, environment variables,
  configuration files, and default values. The environment variable name is
  the full name of the option in uppercase, with all non-alphanumeric
  characters replaced by underscores. For example, the environment variable
  `FOO_BAR` sets the option `foo.bar`. Users may also override the default
  name by putting the environment name after the short names, separated by a
  comma. For example, `opt_group{custom_options_, "foo"}.add("bar,b,MY_BAR")`
  overrides the default environment variable name `FOO_BAR` with `MY_BAR`.
- The formatting functions from `caf/chrono.hpp` now support precision for the
  fractional part of the seconds and an option to print with a fixed number of
  digits.
- The new class `caf::chunk` represents an immutable sequence of bytes with a
  fixed size. Unlike `std::span`, a `chunk` owns its data and can be (cheaply)
  copied and moved.
- Users can now convert state classes with a `make_behavior` member function
  into a "function-based actor" via the new `actor_from_state` utility. For
  example, `sys.spawn(caf::actor_from_state<my_state>, args...)` creates a new
  actor that initializes its state with `my_state{args...}` and then calls
  `make_behavior()` on the state object to obtain the initial behavior.
- The `aout` utility received a `println` member function that adds a formatted
  line to the output stream. The function uses the same formatting backend as
  the logging API.

### Fixed

- The class `caf::test::outline` is now properly exported from the test module.
  This fixes builds with dynamic linking against `libcaf_test`.
- Fix a crash with the new deterministic test fixture when cleaning up actors
  with stashed messages (#1589).
- When using `request(...).await(...)`, the actor no longer suspends handling of
  system messages while waiting for the response (#1584).
- Fix compiler errors and warnings for GCC 13 in C++23 mode (#1583).
- Fix encoding of chunk size in chunked HTTP responses (#1587).
- Fix leak when using `spawn_inactive` and not launching the actor explicitly
  (#1597).
- Fix a minor bug in the deserialization of messages that caused CAF to allocate
  more storage than necessary (#1614).
- Add missing `const` to `publisher<T>::observe_on`.
- All `observable` implementations now properly call `on_subscribe` on their
  subscriber before calling `on_error`.
- The function `config_value::parse` now properly handles leading and trailing
  whitespaces.
- Comparing two  `caf::unordered_flat_map` previously relied on the order of
  elements in the map and thus could result in false negatives. The new
  implementation is correct and no longer relies on the order of elements.
- When using `--dump-config`, CAF now properly renders nested dictionaries.
  Previously, dictionaries in lists missed surrounding braces.
- CAF now parses `foo.bar = 42` in a config file as `foo { bar = 42 }`, just as
  it does for CLI arguments.
- Fix shutdown logic for actors with open streams. This resolves an issue where
  actors would not terminate correctly after receiving an exit message (#1657).
- Fix compilation error on MSVC when building `caf_test` with shared libraries
  enabled (#1669).
- Calling `delay_for_fn` on a flow coordinator now returns a `disposable` in
  order to be consistent with `delay_for` and `delay_until`.
- Calling `dispose` on a server (e.g. an HTTP server) now properly closes all
  open connections.
- Fix static assert in `expected` when calling `transform` on an rvalue with a
  function object that only accepts an rvalue.
- The function `caf::net::make_pipe` no longer closes read/write channels of the
  connected socket pair on Windows. This fixes a bug where the pipe would close
  after two minutes of inactivity.

### Changed

- Calling `to_string` on any of CAF's enum types now represents the enum value
  using the short name instead of the fully qualified name. For example,
  `to_string(sec::none)` now returns `"none"` instead of `"caf::sec::none"`.
  Accordingly, `from_string` now accepts the short name (in additional to the
  fully qualified name).
- Log format strings no longer support `%C`. CAF still recognizes this option
  but it will always print `null`.
- The function `caf::telemetry::counter::inc` now allows passing 0 as an
  argument. Previously, passing 0 triggered an assertion when building CAF with
  runtime checks enabled.
- Calling `dispose` on a flow subscription now calls `on_error(sec::disposed)`
  on the observer. Previously, CAF would simply call `on_complete()` on the
  observer, making it impossible to distinguish between a normal completion and
  disposal.
- The `caf::logger` received a complete overhaul and became an interface class.
  By turning the class into an interface, users can now install custom logger
  implementations. CAF uses the previous implementation as the default logger if
  no custom logger is configured. To install a logger, users can call
  `cfg.logger_factory(my_logger_factory)` on the `actor_system_config` before
  constructing the `actor_system`. The logger factory is a function object with
  signature `caf::intrusive_ptr<caf::logger>(caf::actor_system&)`. Furthermore,
  log messages are now formatted using `std::format` when compiling CAF with
  C++20 or later. Otherwise, CAF will fall back to a minimal formatting
  implementation with compatible syntax. The logging API will also automatically
  convert any type with a suitable `inspect` overload to a string if the type is
  not recognized by `format`.

### Deprecated

- Calling `to_stream` or `to_typed_stream` on an actor is now deprecated. Simply
  call `to_stream` or `to_typed_stream` directly on the `observable` instead.

### Removed

- The implementation of `operator<` for `caf::unordered_flat_map` was broken and
  relied on the order of elements in the map. We have removed it, since it has
  never worked correctly and a correct implementation would be too expensive.

## [0.19.4] - 2023-09-29

### Fixed

- Stream batches are now properly serialized and deserialized when subscribing
  to a stream from a remote node (#1566).

## [0.19.3] - 2023-09-20

### Added

- The class `caf::telemetry::label` now has a new `compare` overload that
  accepts a `caf::telemetry::label_view` to make the interface of the both
  classes symmetrical.
- The template class `caf::dictionary` now has new member functions for erasing
  elements.
- The CLI argument parser now supports a space separator when using long
  argument names, e.g., `--foo bar`. This is in addition to the existing
  `--foo=bar` syntax.
- The functions `make_message` and `make_error` now support `std::string_view`
  as input and automatically convert it to `std::string`.
- To make it easier to set up asynchronous flows, CAF now provides a new class:
  `caf::async::publisher`. Any observable can be transformed into a publisher by
  calling `to_publisher`. The publisher can then be used to subscribe to the
  observable from other actors or threads. The publisher has only a single
  member function: `observe_on`. It converts the publisher back into an
  observable. This new abstraction allows users to set up asynchronous flows
  without having to manually deal with SPSC buffers.
- The flow API received additional operators: `first`, `last`, `take_last`,
  `skip_last`, `element_at`, and `ignore_elements`.

### Changed

- When using CAF to parse CLI arguments, the output of `--help` now includes all
  user-defined options. Previously, only options in the global category or
  options with a short name were included. Only CAF options are now excluded
  from the output. They will still be included in the output of `--long-help`.
- The output of `--dump-config` now only contains CAF options from loaded
  modules. Previously, it also included options from modules that were not
  loaded.
- We renamed `caf::flow::item_publisher` to `caf::flow::multicaster` to better
  reflect its purpose and to avoid confusion with the new
  `caf::async::publisher`.
- When failing to deserialize a request, the sender will receive an error of
  kind `sec::malformed_message`.
- When implementing custom protocol layers that sit on top of an octet stream,
  the `delta` byte span passed to `consume` now resets whenever returning a
  positive value from `consume`.
- When constructing a `behavior` or `message_handler`, callbacks that take a
  `message` argument are now treated as catch-all handlers.
- When creating a message with a non-existing type ID, CAF now prints a
  human-readable error message and calls `abort` instead of crashing the
  application.

### Fixed

- Fix build errors with exceptions disabled.
- Fix a regression in `--dump-config` that caused CAF applications to emit
  malformed output.
- Fix handling of WebSocket frames that are exactly on the 65535 byte limit.
- Fix crash when using a fallback value for optional values (#1427).
- Using `take(0)` on an observable now properly creates an observable that calls
  `on_complete` on its subscriber on the first activity of the source
  observable. Previously, the created observable would never reach its threshold
  and attempt to buffer all values indefinitely.
- The comparison operator of `intrusive_ptr` no longer accidentally creates new
  `intrusive_ptr` instances when comparing to raw pointers.
- Fix function object lifetimes in actions. This bug caused CAF to hold onto a
  strong reference to actors that canceled a timeout until the timeout expired.
  This could lead to actors being kept alive longer than necessary (#698).
- Key lookups in `caf::net::http::request_header` are now case-insensitive, as
  required by the HTTP specification. Further, `field_at` is now a `const`
  member function (#1554).

## [0.19.2] - 2023-06-13

### Changed

- Install CAF tools to `${CMAKE_INSTALL_BINDIR}` to make packaging easier.
- The OpenSSL module no longer hard-codes calls to `SSL_CTX_set_cipher_list` in
  order to use the system settings by default. Users can provide a custom cipher
  list by providing a value for the configuration option
  `caf.openssl.cipher-list`. To restore the previous behavior, set this
  parameter to `HIGH:!aNULL:!MD5` when running with a certificate and
  `AECDH-AES256-SHA@SECLEVEL=0` otherwise (or without `@SECLEVEL=0` for older
  versions of OpenSSL). Please note that these lists are *not* recommended as
  safe defaults, which is why we are no longer setting these values.

### Fixed

- Add missing initialization code for the new caf-net module when using the
  `CAF_MAIN` macro. This fixes the `WSANOTINITIALISED` error on Windows (#1409).
- The WebSocket implementation now properly re-assembles fragmented frames.
  Previously, a bug in the framing protocol implementation caused CAF to sever
  the connection when encountering continuation frames (#1417).

### Added

- Add new utilities in `caf::chrono` for making it easier to handle ISO 8601
  timestamps. The new function `std::chrono::to_string` converts system time to
  an ISO timestamp. For reading an ISO timestamp, CAF now provides the class
  `caf::chrono::datetime`. It can parse ISO-formatted strings via `parse` (or
  `datetime::from_string`) and convert them to a local representation via
  `to_local_time`. Please refer to the class documentation for more details.

## [0.19.1] - 2023-05-01

### Added

- The class `json_value` can now hold unsigned 64-bit integer values. This
  allows it to store values that would otherwise overflow a signed integer.
  Values that can be represented in both integer types will return `true` for
  `is_integer()` as well as for the new `is_unsigned()` function. Users can
  obtain the stored value as `uint64_t` via `to_unsigned()`.

### Changed

- With the addition of the unsigned type to `json_value`, there is now a new
  edge case where `is_number()` returns `true` but neither `is_integer()` nor
  `is_double()` return `true`: integer values larger than `INT64_MAX` will only
  return true for `is_unsigned()`.

### Fixed

- Fix flow setup for servers that use `web_socket::with`. This bug caused
  servers to immediately abort incoming connection (#1402).
- Make sure that a protocol stack ships pending data before closing a socket.
  This bug prevented clients from receiving error messages from servers if the
  server shuts down immediately after writing the message.

## [0.19.0] - 2023-04-17

### Added

- The new classes `json_value`, `json_array` and `json_object` allow working
  with JSON inputs directly. Actors can also pass around JSON values safely.
- Futures can now convert to observable values for making it easier to process
  asynchronous results with data flows.
- Add new `*_weak` variants of `scheduled_actor::run_{delayed, scheduled}`.
  These functions add no reference count to their actor, allowing it to become
  unreachable if other actors no longer reference it.
- Typed actors that use a `typed_actor_pointer` can now access the
  `run_{delayed,scheduled}` member functions.
- Scheduled and delayed sends now return a disposable (#1362).
- Typed response handles received support for converting them to observable or
  single objects.
- Typed actors that use the type-erased pointer-view type received access to the
  new flow API functions (e.g., `make_observable`).
- Not initializing the meta objects table now prints a diagnosis message before
  aborting the program. Previously, the application would usually just crash due
  to a `nullptr`-access inside some CAF function.
- The class `expected` now implements the monadic member functions from C++23
  `std::expected` as well as `value_or`.

### Changed

- After collecting experience and feedback on the new HTTP and WebSocket APIs
  introduced with 0.19.0-rc.1, we decided to completely overhaul the
  user-facing, high-level APIs. Please consult the manual for the new DSL to
  start servers.

### Fixed

- When exporting metrics to Prometheus, CAF now normalizes label names to meet
  the Prometheus name requirements, e.g., `label-1` becomes `label_1` (#1386).
- The SPSC buffer now makes sure that subscribers get informed of a producer has
  already left before the subscriber appeared and vice versa. This fixes a race
  on the buffer that could cause indefinite hanging of an application.
- Fused stages now properly forward errors during the initial subscription to
  their observer.
- The `fan_out_request` request now properly deals with actor handles that
  respond with `void` (#1369).
- Fix subscription and event handling in flow buffer operator.
- The `mcast` and `ucast` operators now stop calling `on_next` immediately when
  disposed.
- Actors no longer terminate despite having open streams (#1377).
- Actors reading from external sources such as SPSC buffers via a local flow
  could end up in a long-running read loop. To avoid potentially starving other
  actors or activities, scheduled actors now limit the amount of actions that
  may run in one iteration (#1364).
- Destroying a consumer or producer resource before opening it lead to a stall
  of the consumer / producer. The buffer now keeps track of whether `close` or
  `abort` were called prior to consumers or producers attaching.
- The function `caf::net::make_tcp_accept_socket` now handles passing `0.0.0.0`
  correctly by opening the socket in IPv4 mode. Passing an empty bind address
  now defaults to `INADDR6_ANY` (but allowing IPv4 clients) with `INADDR_ANY` as
  fallback in case opening the socket in IPv6 mode failed.
- Add missing includes that prevented CAF from compiling on GCC 13.
- Fix AddressSanitizer and LeakSanitizer findings in some flow operators.

### Deprecated

- All member functions from `caf::expected` that have no equivalent in
  `std::expected` are now deprecated. Further, `caf::expected<unit_t>` as well
  as constructing from `unit_t` are deprecated as well. The reasoning behind
  this decision is that `caf::expected` should eventually become an alias for
  `std::expected<T, caf::error>`.

## [0.19.0-rc.1] - 2022-10-31

### Added

- CAF now ships an all-new "flow API". This allows users to express data flows
  at a high level of abstraction with a ReactiveX-style interface. Please refer
  to new examples and the documentation for more details, as this is a large
  addition that we cannot cover in-depth here.
- CAF has received a new module: `caf.net`. This module enables CAF applications
  to interface with network protocols more directly than `caf.io`. The new
  module contains many low-level building blocks for implementing bindings to
  network protocols. However, CAF also ships ready-to-use, high-level APIs for
  WebSocket and HTTP. Please have a look at our new examples that showcase the
  new APIs!
- To complement the flow API as well as the new networking module, CAF also
  received a new set of `async` building blocks. Most notably, this includes
  asynchronous buffers for the flow API and futures / promises that enable the
  new HTTP request API. We plan on making these building blocks more general in
  the future for supporting a wider range of use cases.
- JSON inspectors now allow users to use a custom `type_id_mapper` to generate
  and parse JSON text that uses different names for the types than the C++ API.

### Fixed

- Passing a response promise to a run-delayed continuation could result in a
  heap-use-after-free if the actor terminates before the action runs. The
  destructor of the promise now checks for this case.
- Accessing URI fields now always returns the normalized string.
- Module options (e.g. for the `middleman`) now show up in `--long-help` output.
- Fix undefined behavior in the Qt group chat example (#1336).
- The `..._instance` convenience functions on the registry metric now properly
  support `double` metrics and histograms.
- The spinlock-based work-stealing implementation had severe performance issues
  on Windows in some cases. We have switched to a regular, mutex-based approach
  to avoid performance degradations. The new implementation also uses the
  mutexes for interruptible waiting on the work queues, which improves the
  responsiveness of the actor system (#1343).

### Changed

- Remote spawning of actors is no longer considered experimental.
- The output of `--dump-config` now prints valid config file syntax.
- When starting a new thread via CAF, the thread hooks API now receives an
  additional tag that identifies the component responsible for launching the new
  thread.
- Response promises now hold a strong reference to their parent actor to avoid
  `broken_promise` errors in some (legitimate) edge cases (#1361).
- The old, experimental `stream` API in CAF has been replaced by a new API that
  is based on the new flow API.

### Deprecated

- The obsolete meta-programming utilities `replies_to` and `reacts_to` no longer
  serve any purpose and are thus deprecated.
- The types `caf::byte`, `caf::optional` and `caf::string_view` became obsolete
  after switching to C++17. Consequently, these types are now deprecated in
  favor of their standard library counterpart.
- The group-based pub/sub mechanism never fit nicely into the typed messaging
  API and the fact that group messages use the regular mailbox makes it hard to
  separate regular communication from multi-cast messages. Hence, we decided to
  drop the group API and instead focus on the new flows and streams that can
  replace group-communication in many use cases.
- The "actor-composition operator" was added as a means to enable the first
  experimental streaming API. With that gone, there's no justification to keep
  this feature. While it has some neat niche-applications, the prevent some
  optimization we would like to apply to the messaging layer. Hence, we will
  remove this feature without a replacement.

### Removed

- The template type `caf::variant` also became obsolete when switching to C++17.
  Unfortunately, the implementation was not as standalone as its deprecated
  companions and some of the free functions like `holds_alternative` were too
  greedy and did not play nicely with ADL when using `std::variant` in the same
  code base. Since fixing `caf::variant` does not seem to be worth the time
  investment, we remove this type without a deprecation cycle.

## [0.18.7] - 2023-02-08

### Fixed

- The JSON parser no longer chokes when encountering `null` as last value before
  the closing parenthesis.
- The JSON reader now automatically widens integers to doubles as necessary.
- Parsing deeply nested JSON inputs no longer produces a stack overflow.
  Instead, the parser rejects any JSON with too many nesting levels.
- The `fan_out_request` request now properly deals with actor handles that
  respond with `void` (#1369). Note: back-ported fix from 0.19.

## [0.18.6] - 2022-03-24

### Added

- When adding CAF with exceptions enabled (default), the unit test framework now
  offers new check macros:
  - `CAF_CHECK_NOTHROW(expr)`
  - `CAF_CHECK_THROWS_AS(expr, type)`
  - `CAF_CHECK_THROWS_WITH(expr, str)`
  - `CAF_CHECK_THROWS_WITH_AS(expr, str, type)`

### Fixed

- The DSL function `run_until` miscounted the number of executed events, also
  causing `run_once` to report a wrong value. Both functions now return the
  correct result.
- Using `allow(...).with(...)` in unit tests without a matching message crashed
  the program. By adding a missing NULL-check, `allow` is now always safe to
  use.
- Passing a response promise to a run-delayed continuation could result in a
  heap-use-after-free if the actor terminates before the action runs. The
  destructor of the promise now checks for this case.
- Fix OpenSSL 3.0 warnings when building the OpenSSL module by switching to
  newer EC-curve API.
- When working with settings, `put`, `put_missing`, `get_if`, etc. now
  gracefully handle the `global` category when explicitly using it.
- Messages created from a `message_builder` did not call the destructors for
  their values, potentially causing memory leaks (#1321).

### Changed

- Since support of Qt 5 expired, we have ported the Qt examples to version 6.
  Hence, building the Qt examples now requires Qt in version 6.
- When compiling CAF with exceptions enabled (default), `REQUIRE*` macros,
  `expect` and `disallow` no longer call `abort()`. Instead, they throw an
  exception that only stops the current test instead of stopping the entire test
  program.
- Reporting of several unit test macros has been improved to show correct line
  numbers and provide better diagnostic of test errors.

## [0.18.5] - 2021-07-16

### Fixed

- 0.18.4 introduced a potential crash when using the OpenSSL module and
  encountering `SSL_ERROR_WANT_READ`. The crash manifested if CAF resumed a
  write operation but failed to fully reset its state. The state management (and
  consequently the crash) has been fixed.
- CAF now clears the actor registry before calling the destructors of loaded
  modules. This fixes undefined behavior that could occur in rare cases where
  actor cleanup code could run after loaded modules had been destroyed.

## [0.18.4] - 2021-07-07

### Added

- The new class `caf::telemetry::importer::process` allows users to get access
  to process metrics even when not configuring CAF to export metrics to
  Prometheus via HTTP.

### Changed

- Message views now perform the type-check in their constructor. With this
  change, the `make_*` utility functions are no longer mandatory and users may
  instead simply construct the view directly.

### Fixed

- Printing a `config_value` that contains a zero duration `timespan` now
  properly prints `0s` instead of `1s` (#1262). This bug most notably showed up
  when setting a `timespan` parameter such as `caf.middleman.heartbeat-interval`
  via config file or CLI to `0s` and then printing the config parameter, e.g.,
  via `--dump-config`.
- Blocking actors now release their private thread before decrementing the
  running-actors count to resolve a race condition during system shutdown that
  could result in the system hanging (#1266).
- When using the OpenSSL module, CAF could run into a state where the SSL layer
  wants to read data while CAF is trying to send data. In this case, CAF did not
  properly back off, causing high CPU load due to spinning and in some scenarios
  never recovering. This issue has been resolved by properly handling
  `SSL_ERROR_WANT_READ` on the transport (#1060).
- Scheduled actors now accept default handlers for down messages etc. with
  non-const apply operator such as lambda expressions declared as `mutable`.

### Removed

- Dropped three obsolete (and broken) macros in the `unit_test.hpp` header:
  `CAF_CHECK_FAILED`, `CAF_CHECK_FAIL` and `CAF_CHECK_PASSED`.

## [0.18.3] - 2021-05-21

### Added

- The `actor_system_config` now has an additional member called
  `config_file_path_alternatives`. With this, users can configure fallback paths
  for locating a configuration file. For example, an application `my-app` on a
  UNIX-like system could set `config_file_path` to `my-app.conf` and then add
  `/etc/my-app.conf` to `config_file_path_alternatives` in order to follow the
  common practice of looking into the current directory first before looking for
  a system-wide configuration file.

### Changed

- Counters in histogram buckets are now always integers, independently on the
  value type of the histogram. Buckets can never increase by fractional values.

### Deprecated

- All `parse` function overloads in `actor_system_config` that took a custom
  configuration file path as argument were deprecated in favor of consistently
  asking users to use the `config_file_path` and `config_file_path_alternatives`
  member variables instead

### Fixed

- For types that offer implicit type conversion, trying to construct a
  `result<T>` could result in ambiguity since compilers could construct either
  `T` itself or `expected<T>` for calling a constructor of `result<T>`. To fix
  the ambiguity, `result<T>` now accepts any type that allows constructing a `T`
  internally without requiring a type conversion to `T` as an argument (#1245).
- Fix configuration parameter lookup for the `work-stealing` scheduler policy.
- Applications that expose metrics to Prometheus properly terminate now.

## [0.18.2] - 2021-03-26

### Added

- CAF includes two new inspector types for consuming and generating
  JSON-formatted text: `json_writer` and `json_reader`.

### Changed

- Setter functions for fields may now return either `bool`, `caf::error` or
  `void`. Previously, CAF only allowed `bool`.

### Fixed

- Passing a getter and setter pair to an inspector via `apply` produced a
  compiler error for non-builtin types. The inspection API now recursively
  inspects user-defined types instead, as was the original intend (#1216).
- The handle type `typed_actor` now can construct from a `typed_actor_pointer`.
  This resolves a compiler error when trying to initialize a handle for
  `my_handle` from a self pointer of type `my_handle::pointer_view` (#1218).
- Passing a function reference to the constructor of an actor caused a compiler
  error when building with logging enabled. CAF now properly handles this edge
  case and logs such constructor arguments as `<unprintable>` (#1229).
- The CLI parser did not recognize metrics filters. Hence, passing
  `--caf.metrics-filters.actors.includes=...` to a CAF application resulted in
  an error. The `includes` and `excludes` filters are now consistently handled
  and accepted in config files as well as on the command line (#1238).
- Silence a deprecated-enum-conversion warning for `std::byte` (#1230).
- Fix heap-use-after-free when accessing the meta objects table in applications
  that leave the `main` function while the actor system and its worker threads
  are still running (#1241).
- The testing DSL now properly accounts for the message prioritization of actors
  (suspending regular behavior until receiving the response) when using
  `request.await` (#1232).

## [0.18.1] - 2021-03-19

### Fixed

- Version 0.18.0 introduced a regression on the system parameter
  `caf.middleman.heartbeat-interval` (#1235). We have addressed the issue by
  porting the original fix for CAF 0.17.5 (#1095) to the 0.18 series.

## [0.18.0] - 2021-01-25

### Added

- The enum `caf::sec` received an additional error code: `connection_closed`.
- The new `byte_span` and `const_byte_span` aliases provide convenient
  definitions when working with sequences of bytes.
- The base metrics now include four new histograms for illuminating the I/O
  module: `caf.middleman.inbound-messages-size`,
  `caf.middleman.outbound-messages-size`, `caf.middleman.deserialization-time`
  and `caf.middleman.serialization-time`.
- The macro `CAF_ADD_TYPE_ID` now accepts an optional third parameter for
  allowing users to override the default type name.
- The new function pair `get_as` and `get_or` model type conversions on a
  `config_value`. For example, `get_as<int>(x)` would convert the content of `x`
  to an `int` by either casting numeric values to `int` (with bound checks) or
  trying to parse the input of `x` if it contains a string. The function
  `get_or` already existed for `settings`, but we have added new overloads for
  generalizing the function to `config_value` as well.
- The `typed_response_promise` received additional member functions to mirror
  the interface of the untyped `response_promise`.
- Configuration files now allow dot-separated notation for keys. For example,
  users may write `caf.scheduler.max-threads = 4` instead of the nested form
  `caf { scheduler { max-threads = 4 } }`.

### Deprecated

- The new `get_as` and `get_or` function pair makes type conversions on a
  `config_value` via `get`, `get_if`, etc. obsolete. We will retain the
  STL-style interface for treating a `config_value` as a `variant`-like type.

### Changed

- When using `CAF_MAIN`, CAF now looks for the correct default config file name,
  i.e., `caf-application.conf`.
- Simplify the type inspection API by removing the distinction between
  `apply_object` and `apply_value`. Instead, inspectors only offer `apply` and
  users may now also call `map`, `list`, and `tuple` for unboxing simple wrapper
  types. Furthermore, CAF no longer automatically serializes enumeration types
  using their underlying value because this is fundamentally unsafe.
- CAF no longer parses the input to string options on the command line. For
  example, `my_app '--msg="hello"'` results in CAF storing `"hello"` (including
  the quotes) for the config option `msg`. Previously, CAF tried to parse any
  string input on the command-line that starts with quotes in the same way it
  would parse strings from a config file, leading to very unintuitive results in
  some cases (#1113).
- Response promises now implicitly share their state when copied. Once the
  reference count for the state reaches zero, CAF now produces a
  `broken_promise` error if the actor failed to fulfill the promise by calling
  either `dispatch` or `delegate`.

### Fixed

- Setting an invalid credit policy no longer results in a segfault (#1140).
- Version 0.18.0-rc.1 introduced a regression that prevented CAF from writing
  parameters parsed from configuration files back to variables. The original
  behavior has been restored, i.e., variables synchronize with user input from
  configuration files and CLI arguments (#1145).
- Restore correct functionality of `middleman::remote_lookup` (#1146). This
  fixes a regression introduced in version 0.18.0-rc.1
- Fixed an endless recursion when using the `default_inspector` from `inspect`
  overloads (#1147).
- CAF 0.18 added support for `make_behavior` in state classes. However, CAF
  erroneously picked this member function over running the function body when
  spawning function-based actors (#1149).
- When passing `nullptr` or custom types with implicit conversions to
  `const char*` to `deep_to_string`, CAF could run into a segfault in the former
  case or do unexpected things in the latter case. The stringification inspector
  now matches precisely on pointer types to stop the compiler from doing
  implicit conversions in the first place.
- Building executables that link to CAF on 32-bit Linux versions using GCC
  failed due to undefined references to `__atomic_fetch` symbols. Adding a CMake
  dependency for `caf_core` to libatomic gets executables to compile and link as
  expected (#1153).
- Fixed a regression for remote groups introduced in 0.18.0-rc.1 (#1157).
- CAF 0.18 introduced the option to set different `excluded-components` filters
  for file and console log output. However, CAF rejected all events that matched
  either filter. The new implementation uses the *intersection* of both filters
  to reject log messages immediately (before enqueueing it to the logger's
  queue) and then applies the filters individually when generating file or
  console output.
- Fix memory leaks when deserializing URIs and when detaching the content of
  messages (#1160).
- Fix undefined behavior in `string_view::compare` (#1164).
- Fix undefined behavior when passing `--config-file=` (i.e., without actual
  argument) to CAF applications (#1167).
- Protect against self-assignment in a couple of CAF classes (#1169).
- Skipping high-priority messages resulted in CAF lowering the priority to
  normal. This unintentional demotion has been fixed (#1171).
- Fix undefined behavior in the experimental datagram brokers (#1174).
- Response promises no longer send empty messages in response to asynchronous
  messages.
- `CAF_ADD_TYPE_ID` now works with types that live in namespaces that also exist
  as nested namespace in CAF such as `detail` or `io` (#1195).
- Solved a race condition on detached actors that blocked ordinary shutdown of
  actor systems in some cases (#1196).

## [0.18.0-rc.1] - 2020-09-09

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
- In preparation of potential future API additions/changes, CAF now includes an
  RFC4122-compliant `uuid` class.
- The new trait class `is_error_code_enum` allows users to enable conversion of
  custom error code enums to `error` and `error_code`.
- CAF now enables users to tap into internal CAF metrics as well as adding their
  own instrumentation! Since this addition is too large to cover in a changelog
  entry, please have a look at the new *Metrics* Section of the manual to learn
  more.

### Deprecated

- The `to_string` output for `error` now renders the error code enum by default.
  This renders the member functions `actor_system::render` and
  `actor_system_config::render` obsolete.
- Actors that die due to an unhandled exception now use `sec::runtime_error`
  consistently. This makes `exit_reason::unhandled_exception` obsolete.

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
- A `stateful_actor` now forwards excess arguments to the `State` rather than to
  the `Base`. This enables states with non-default constructors. When using
  `stateful_actor<State>` as pointer type in function-based actors, nothing
  changes (i.e. the new API is backwards compatible for this case). However,
  calling `spawn<stateful_actor<State>>(xs...)` now initializes the `State` with
  the argument pack `xs...` (plus optionally a `self` pointer as first
  argument). Furthermore, the state class can now provide a `make_behavior`
  member function to initialize the actor (this has no effect for function-based
  actors).
- In order to stay more consistent with naming conventions of the standard
  library, we have renamed some values of the `pec` enumeration:
  + `illegal_escape_sequence` => `invalid_escape_sequence`
  + `illegal_argument` => `invalid_argument`
  + `illegal_category` => `invalid_category`
- CAF no longer automagically flattens `tuple`, `optional`, or `expected` when
  returning these types from message handlers. Users can simply replace
  `std::tuple<A, B, C>` with `caf::result<A, B, C>` for returning more than one
  value from a message handler.
- A `caf::result` can no longer represent `skip`. Whether a message gets skipped
  or not is now only for the default handler to decide. Consequently, default
  handlers now return `skippable_result` instead of `result<message>`. A
  skippable result is a variant over `delegated<message>`, `message`, `error`,
  or `skip_t`. The only good use case for message handlers that skip a message
  in their body was in typed actors for getting around the limitation that a
  typed behavior always must provide all message handlers (typed behavior assume
  a complete implementation of the interface). This use case received direct
  support: constructing a typed behavior with `partial_behavior_init` as first
  argument suppresses the check for completeness.
- In order to reduce complexity of typed actors, CAF defines interfaces as a set
  of function signatures rather than using custom metaprogramming facilities.
  Function signatures *must* always wrap the return type in a `result<T>`. For
  example: `typed_actor<result<double>(double)>`. We have reimplemented the
  metaprogramming facilities `racts_to<...>` and `replies_to<...>::with<...>`
  as an alternative way of writing the function signature.
- All parsing functions in `actor_system_config` that take an input stream
  exclusively use the new configuration syntax (please consult the manual for
  details and examples for the configuration syntax).
- The returned string of `name()` must not change during the lifetime of an
  actor. Hence, `stateful_actor` now only considers static `name` members in its
  `State` for overriding this function. CAF always assumed names belonging to
  *types*, but did not enforce it because the name was only used for logging.
  Since the new metrics use this name for filtering now, we enforce static names
  in order to help avoid hard-to-find issues with the filtering mechanism.
- The type inspection API received a complete overhaul. The new DSL for writing
  `inspect` functions exposes the entire structure of an object to CAF. This
  enables inspectors to read and write a wider range of data formats. In
  particular human-readable, structured data such as configuration files, JSON,
  XML, etc. The inspection API received too many changes to list them here.
  Please refer to the manual section on type inspection instead.

### Removed

- A vendor-neutral API for GPGPU programming sure sounds great. Unfortunately,
  OpenCL did not catch on in the way we had hoped. At this point, we can call
  OpenCL dead and gone. There is only legacy support available and recent
  versions of the standard were never implemented in the first place.
  Consequently, we've dropped the `opencl` module.
- The old `duration` type is now superseded by `timespan` (#994).
- The enum `match_result` became obsolete. Individual message handlers can no
  longer skip messages. Hence, message handlers can only succeed (match) or not.
  Consequently, invoking a message handler or behavior now returns a boolean.
- All member functions of `scheduled_actor` for adding stream managers (such as
  `make_source`) were removed in favor their free-function equivalent, e.g.,
  `attach_stream_source`
- The configuration format of CAF has come a long way since first starting to
  allow user-defined configuration via `.ini` files. Rather than sticking with
  the weird hybrid that evolved over the years, we finally get rid of the last
  pieces of INI syntax and go with the much cleaner, scoped syntax. The new
  default file name for configuration files is `caf-application.conf`.

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
- The `typed_actor_view` decorator lacked several member functions such as
  `link_to`, `send_exit`, etc. These are now available.
- Constructing a `typed_actor` handle from a pointer view failed due to a
  missing constructor overload. This (explicit) overload now exists and the
  conversion should work as expected.
- Sending floating points to remote actors changed `infinity` and `NaN` to
  garbage values (#1107). The fixed packing / unpacking routines for IEEE 754
  values keep these non-numeric values intact now. It is worth mentioning that
  the new algorithm downgrades signaling NaN values to silent NaN values,
  because the standard API does not provide predicates to distinguish between the
  two. This should have no implications for real-world applications, because
  actors that produce a signaling NaN trigger trap handlers before sending
  the result to another actor.
- The URI parser stored IPv4 addresses as strings (#1123). Users can now safely
  assume that the parsed URI for `tcp://127.0.0.1:8080` returns an IP address
  when calling `authority().host`.

## [0.17.7] - Unreleased

### Fixed

- Datagram servants of UDP socket managers were not added as children to their
  parent broker on creation, which prevented proper system shutdown in some
  cases. Adding all servants consistently to the broker should make sure UDP
  brokers terminate correctly (#1133).
- Backport stream manager fix from CAF 0.18 for fused downstream managers that
  prevent loss of messages during regular actor shutdown.

## [0.17.6] - 2020-07-24

### Fixed

- Trying to connect to an actor published via the OpenSSL module with the I/O
  module no longer hangs indefinitely (#1119). Instead, the OpenSSL module
  immediately closes the socket if initializing the SSL session fails.

## [0.17.5] - 2020-05-13

### Added

- In order to allow users to start migrating towards upcoming API changes, CAF
  0.17.5 includes a subset of the CAF 0.18 `type_id` API. Listing all
  user-defined types between `CAF_BEGIN_TYPE_ID_BLOCK` and
  `CAF_END_TYPE_ID_BLOCK` assigns ascending type IDs. Only one syntax for
  `CAF_ADD_ATOM` exists, since the atom text is still mandatory. Assigning type
  IDs has no immediate effect by default. However, the new function
  `actor_system_config::add_message_types` accepts an ID block and adds
  runtime-type information for all types in the block.
- In order to opt into the compile-time checks for all message types, users can
  set the `CAF_ENABLE_TYPE_ID_CHECKS` CMake flag to `ON` (pass
  `--enable-type-id-checks` when using the `configure` script). Building CAF
  with this option causes compiler errors when sending a type without a type ID.
  This option in conjunction with the new `add_message_types` function removes a
  common source of bugs: forgetting to call `add_message_type<T>` for all types
  that can cross the wire.

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
- Fix build on GCC 7.2
- Fix build error in the OpenSSL module under some MSVC configurations
- Serializer and deserializer now accept `std::chrono::time_point` for all clock
  types instead of hard-wiring `std::system_clock`.
- In some edge cases, actors failed to shut down properly when hosting a stream
  source (#1076). The handshake process for a graceful shutdown has been fixed.
- Fixed a compiler error on Clang 10 (#1077).
- Setting lists and dictionaries on the command line now properly overrides
  default values and values from configuration files instead of appending to
  them (#942).
- Using unquoted strings in command-line arguments inside lists now works as
  expected. For example, `--foo=abc,def` is now equivalent to
  `--foo=["abc", "def"]`.
- Fixed a type mismatch in the parameter `middleman.heartbeat-interval` (#1095).
  CAF consistently uses `timespan` for this parameter now.

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

[1.0.2]: https://github.com/actor-framework/actor-framework/releases/1.0.2
[1.0.1]: https://github.com/actor-framework/actor-framework/releases/1.0.1
[1.0.0]: https://github.com/actor-framework/actor-framework/releases/1.0.0
[0.19.5]: https://github.com/actor-framework/actor-framework/releases/0.19.5
[0.19.4]: https://github.com/actor-framework/actor-framework/releases/0.19.4
[0.19.3]: https://github.com/actor-framework/actor-framework/releases/0.19.3
[0.19.2]: https://github.com/actor-framework/actor-framework/releases/0.19.2
[0.19.1]: https://github.com/actor-framework/actor-framework/releases/0.19.1
[0.19.0]: https://github.com/actor-framework/actor-framework/releases/0.19.0
[0.19.0-rc.1]: https://github.com/actor-framework/actor-framework/releases/0.19.0-rc.1
[0.18.7]: https://github.com/actor-framework/actor-framework/releases/0.18.7
[0.18.6]: https://github.com/actor-framework/actor-framework/releases/0.18.6
[0.18.5]: https://github.com/actor-framework/actor-framework/releases/0.18.5
[0.18.4]: https://github.com/actor-framework/actor-framework/releases/0.18.4
[0.18.3]: https://github.com/actor-framework/actor-framework/releases/0.18.3
[0.18.2]: https://github.com/actor-framework/actor-framework/releases/0.18.2
[0.18.1]: https://github.com/actor-framework/actor-framework/releases/0.18.1
[0.18.0]: https://github.com/actor-framework/actor-framework/releases/0.18.0
[0.18.0-rc.1]: https://github.com/actor-framework/actor-framework/releases/0.18.0-rc.1
[0.17.7]: https://github.com/actor-framework/actor-framework/compare/0.17.6...release/0.17
[0.17.6]: https://github.com/actor-framework/actor-framework/releases/0.17.6
[0.17.5]: https://github.com/actor-framework/actor-framework/releases/0.17.5
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
