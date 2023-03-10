# Changelog

All notable changes to this project will be documented in this file. The format
is based on [Keep a Changelog](https://keepachangelog.com).

## [Unreleased]

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

### Fixed

- The SPSC buffer now makes sure that subscribers get informed of a producer has
  already left before the subscriber appeared and vice versa. This fixes a race
  on the buffer that could cause indefinite hanging of an application.
- Fused stages now properly forward errors during the initial subscription to
  their observer.
- The `fan_out_request` request now properly deals with actor handles that
  respond with `void` (#1369).
- Fix subscription and event handling in flow buffer operator.
- Fix undefined behavior in getter functions of the flow `mcast` operator.
- Add checks to avoid potential UB when using `prefix_and_tail` or other
  operators that use the `ucast` operator internally.
- The `mcast` and `ucast` operators now stop calling `on_next` immediately when
  disposed.
- Actors no longer terminate despite having open streams (#1377).
- Actors reading from external sources such as SPSC buffers via a local flow
  could end up in a long-running read loop. To avoid potentially starving other
  actors or activities, scheduled actors now limit the amount of actions that
  may run in one iteration (#1364).

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
- Constructing a `typed_actor` handle from a pointer view failed du to a missing
  constructor overload. This (explicit) overload now exists and the conversion
  should work as expected.
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

[Unreleased]: https://github.com/actor-framework/actor-framework/compare/0.19.0-rc.1...master
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
