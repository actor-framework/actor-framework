Version 0.4.2
-------------

__2012-10-1__

- Bugfix: evaluate `errno` whenever select() fails and handle errors properly
- Refactored announce
  * accept recursive containers, e.g.,  vector<vector<double>>
  * allow user-defined types as members of announced types
  * all-new, policy-based implementation
- Use `poll` rather than `select` in middleman (based on the patch by ArtemGr)

Version 0.4.1
-------------

__2012-08-22__

- Bugfix: shutdown() caused segfault if no scheduler or middleman was started

Version 0.4
-----------

__2012-08-20__

- New network layer implementation
- Added acceptor and input/output stream interfaces
- Added overload for publish() and remote_actor() using the new interfaces
- Changed group::add_module to take unique_ptr rather than a raw pointer
- Refactored serialization process for group_ptr
- Changed anyonymous groups to use the implementation of the "local" module
- Added scheduled_and_hidden policy for system-internal, event-based actors
- Enabled serialization of floating point values
- Added shutdown() function
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
- Relaxed definition of become() to accept const lvalue references as well

Version 0.2
-----------

__2012-06-29__

- Removed become_void() [use quit() instead]
- Renamed "future_send()" to "delayed_send()"
- Removed "stacked_actor"; moved functionality to "event_based_actor"
- Renamed "fsm_actor" to "sb_actor"
- Refactored "spawn": spawn(new T(...)) => spawn<T>(...)
- Implemented become()/unbecome() for context-switching & thread-mapped actors
- Moved become()/unbecome() to local_actor
- Ported libcppa from <ucontext.h> to Boost.Context library
