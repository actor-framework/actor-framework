Version 0.8
-----------

__2013_10_01__

- Added support for typed actors
- Added `brokers`: actors that encapsulate networking
- timed_* function family now correctly handles priorities
- Fixed monitoring of remote actors
- Added new exit reason `user_shutdown`
- Deprecated `reply` function and recommend returning values instead
- Provide operator-> for optional
- New class `optional_variant` with visitor-based API
- Added manual to Git repository
- Added new libCURL example
- Bugfixes

Version 0.7.2
-------------

__2013_09_27__

- Fixed a bug in the chaining implementation

Version 0.7.1
-------------

__2013_05_21__

- Fixed type name lookup
- Workaround for GCC 4.8 SFINAE
- Do not call grep/ifconfig for host information on Linux
- Fixed printer example in manual (Section 6.1)

Version 0.7
-----------

__2013_05_12__

- Default actor impl. is now event-based
- Unified spawn options
- Blocking API still available, but opt-in
- Priority-based messaging (opt-in feature)
- Support for OpenCL-based actors (enable with --with-opencl)
- Changed license to LGPL 2.1
- Added `gval` for guard expression
- Implemented thread-mapped, event-based actors

Version 0.6
-----------

__2013_02_22__

- Added `quit_actor` function to terminate actors
- Added continuation feature to non-blocking API
- Allow functor-only usage of `then` and `await`
- Support for Boost 1.53
- Fixed timing bug in synchronous response handling
- Better diagnostics in unit tests
- Use -O3 for release build, because -O4 is broken in Clang
- Auto-reply `EXITED` to orphaned sync requests
- Added `partial_function::or_else` to concatenate partial functions
- New `timed_sync_send` API with different timeout handling
- Added `on_sync_failure` and `on_sync_timeout` handlers for sync messaging
- Added `skip_message` helper to allow users to skip messages manually

Version 0.5.5
-------------

__2013_01_07__

- Cover `aout` in manual
- Solved bug in chaining implementation
- Added support for Boost.Context 1.51
- Use `memory::create` rather than `new` for all actors
- Simplified invoke process of `match_expr`

Version 0.5.4
-------------

__2012-11-19__

- Update libcppa to work with Boost.Context 1.52
- Fix possible memory corruption in `behavior_stack`
- Fix `fiber` implementaiton if compiling without Boost.Context

Version 0.5.3
-------------

__2012-11-13__

- Improved memory management
  * Use a per-thread memory cache for `recursive_queue_node` and `actor` objects
  * Allocate ~1kb worth ob objects rather than allocating each object with `new`
  * Destroyed objects are returned to the cache and re-used later
- Add `intrusive_fwd_ptr` to support counted pointers for forward declared types
- Qt example for group chat, highlighting `actor_widget_mixin`

Version 0.5.2
-------------

__2012-11-05__

- Fixed Bug in CMake when compiling w/o Boost.Context
- Added `--bulid-static` and `--build-static-only` flags to configure script
- Moved `benchmarks` folder to own repository

Version 0.5.1
-------------

__2012-11-03__

- Added `make_response_handle` which allows an actor to reply to message later
- Replace `continuable_writer` with `continuable_io : continuable_reader`
- No more multiple inheritance in `default_peer` (derives `continuable_io`)
- MM reports IO failures to corresponding objects
- New behavior in default protocol regarding outgoing messages:
  * `default_peer` uses FIFO queue for outgoing messages
  * queue stores messages even if no active connection is available
  * new connections then check for previously enqueued messages first

Version 0.5
-----------

__2012-10-26__

- New logging facility
  * must be enabled using --with-cppa-log-level=LEVEL (TRACE-ERROR)
  * --enable-debug also enables ERROR log level implicitly
  * output files are named libcppa_PID_TIMESTAMP.log
  * log format is Log4j-like
- New middleman (MM) architecture
  * MM multiplexes sockets but no longer knows communication internas
  * new protocol interface encapsulates any communication
  * users can add new communication protocols to MM
  * previously used binary protocol is not called 'DEFAULT'
  * MM provides run_later function to hook code into MM event-loop
- New class: `weak_intrusive_ptr`
  * `ref_counted` has protected destructor to enforce use of `request_deletion`
  * default `request_deletion` calls `delete`
  * `enable_weak_ptr_mixin` overrides `request_deletion` to invalidate weak ptrs
- Fixed issue #75: peers hold weak pointers to proxies (breaks cyclic refs)
- `actor_companion_mixin` allows non-actor classes to communicate as/to actors
- `actor_proxy` became an abstract class; must be implemented for each protocol
- Removed global proxy cache singleton
- `actor_addressing` manages proxies; must be implemented for each protocol
- New factory function: `make_counted` (similar to std::make_shared)
- Bugfix: `reply` matches correct message on nested receives

Version 0.4.2
-------------

__2012-10-1__

- Refactored announce
  * accept recursive containers, e.g.,  `vector<vector<double>>`
  * allow user-defined types as members of announced types
  * all-new, policy-based implementation
- Use `poll` rather than `select` in middleman (based on the patch by ArtemGr)

Version 0.4.1
-------------

__2012-08-22__

- Bugfix: `shutdown` caused segfault if no scheduler or middleman was started

Version 0.4
-----------

__2012-08-20__

- New network layer implementation
- Added acceptor and input/output stream interfaces
- Added overload for `publish` and `remote_actor` using the new interfaces
- Changed group::add_module to take unique_ptr rather than a raw pointer
- Refactored serialization process for group_ptr
- Changed anyonymous groups to use the implementation of the "local" module
- Added scheduled_and_hidden policy for system-internal, event-based actors
- Enabled serialization of floating point values
- Added `shutdown` function
- Implemented broker-based forwarding of local groups for 'pseudo multicast'
- Added `then` and `await` member functions to message_future
- Do not send more than one response message with `reply`

Version 0.3.3
-------------

__2012-08-09__

- Bugfix: serialize message id for synchronous messaging
- Added macro to unit_testing/CMakeLists.txt for less verbose CMake setup
- Added function "forward_to" to enable transparent forwarding of sync requests
- Removed obsolete files (gen_server/* and queue_performances/*)
- Bugfix: avoid possible stack overflow in debug mode for test__spawn
- Added functions "send_tuple", "sync_send_tuple" and "reply_tuple"
- Let "make" fail on first error in dual-build mode
- Added rvalue overload for receive_loop
- Added "delayed_send_tuple" and "delayed_reply_tuple"

Version 0.3.2
-------------

__2012-07-30__

- Bugfix: added 'bool' to the list of announced types

Version 0.3.1
-------------

__2012-07-27__

- Bugfix: always return from a synchronous handler if a timeout occurs
- Bugfix: request next timeout after timeout handler invocation if needed

Version 0.3
-----------

__2012-07-25__

- Implemented synchronous messages
- The function become() no longer accepts pointers
- Provide --disable-context-switching option to bypass Boost.Context if needed
- Configure script to hide CMake details behind a nice interface
- Include "tuple_cast.hpp" in "cppa.hpp"
- Added forwarding header "cppa_fwd.hpp"
- Allow raw read & write operations in synchronization interface
- Group subscriptions are no longer attachables

Version 0.2.1
-------------

__2012-07-02__

- More efficient behavior implementation
- Relaxed definition of `become` to accept const lvalue references as well

Version 0.2
-----------

__2012-06-29__

- Removed `become_void` [use `quit` instead]
- Renamed `future_send` to `delayed_send`
- Removed `stacked_actor`; moved functionality to `event_based_actor`
- Renamed `fsm_actor` to `sb_actor`
- Refactored `spawn`: `spawn(new T(...))` => `spawn<T>(...)`
- Implemented `become` & `unbecome` for context-switching & thread-mapped actors
- Moved `become` & `unbecome` to local_actor
- Ported libcppa from `<ucontext.h>` to `Boost.Context` library
