.. _net-overview:

Overview
========

The networking module offers a high-level, declarative DSL for individual
protocols as well as low-level building blocks for implementing new protocols
and assembling protocol stacks.

When using caf-net for applications, we generally recommend sticking to the
declarative API.

Initializing the Module
-----------------------

The networking module is an optional component of CAF. To use it, users can pass
``caf::net::middleman`` to ``CAF_MAIN``.

When not using the ``CAF_MAIN`` macro, users must initialize the library by:

- Calling ``caf::net::middleman::init_global_meta_objects()`` before
  initializing the ``actor_system``. This step adds the necessary meta objects
  to the global meta object registry.
- Calling ``caf::net::middleman::init_host_system()`` before initializing the
  ``actor_system`` (or calling any function of the module). This step runs
  platform-specific initialization code (such as calling ``WSAStartup`` on
  Windows) by calling ``caf::net::this_host::startup()`` and initializes the SSL
  library by calling ``caf::net::ssl::startup()`` The function
  ``init_host_system`` returns a guard object that calls
  ``caf::net::this_host::cleanup()`` and ``caf::net::ssl::cleanup()`` in its
  destructor.

Declarative High-level DSL :sup:`experimental`
----------------------------------------------

The high-level APIs follow a factory pattern that configures each layer from the
bottom up, usually starting at the actor system. For example:

.. code-block:: C++

   namespace ws = caf::net::web_socket;
   auto conn = ws::with(sys)
                 .connect("localhost", 8080)
                 .start([&sys](auto pull, auto push) {
                   sys.spawn(my_actor, pull, push);
                 });

This code snippet tries to establish a WebSocket connection on
``localhost:8080`` and then spawns an actor that receives messages from the
WebSocket by reading from ``pull`` and sends messages to the WebSocket by
writing to ``push``. Errors are also transmitted over the ``pull`` resource.

A trivial implementation for ``my_actor`` that sends all messages it receives
back to the sender could look like this:

.. code-block:: C++

   namespace ws = caf::net::web_socket;
   void my_actor(caf::event_based_actor* self,
                 ws::default_trait::pull_resource pull,
                 ws::default_trait::push_resource push) {
     self->make_observable().from_resource(pull).subscribe(push);
   }

In general, starting a client that connects to a server follows the pattern:

.. code-block:: C++

   PROTOCOL::with(CONTEXT).connect(WHERE).start(ON_CONNECT);

Starting a server uses ``accept`` instead:

.. code-block:: C++

   PROTOCOL::with(CONTEXT).accept(WHERE).start(ON_ACCEPT);

The ``ON_ACCEPT`` handler may be optional and some protocols do not support it
at all. For example, the HTTP server accepts callbacks for individual routes and
users may call ``start`` without arguments to simply launch the server and
dispatch to the configured routes.

In all cases, CAF returns ``expected<disposable>``. The ``disposable`` allows
users to cancel the background activity at any time. Note: when canceling a
server, this only disposes the server itself. Previously established connections
remain unaffected.

For error reporting, most factories also allow setting a callback with
``do_on_error``. This can be useful for ignoring the result value but still
reporting errors.

Many protocols also accept additional configuration parameters. For example, the
``connect`` API may also allows to configure multiple connection attempts.

Please refer to the sections for the individual protocols for a more detailed
description of available options.

Protocol Layering
-----------------

Layering plays an important role in the design of the ``caf.net`` module. When
implementing a new protocol for ``caf.net``, this protocol should integrate
naturally with any number of other protocols. To enable this key property,
``caf.net`` establishes a set of conventions and abstract interfaces.

Please note that the protocol layering is meant for adding new networking
capabilities to CAF, but we do not recommend using the protocol stacks directly
in application code. The protocol stacks are meant as building blocks for
higher-level, declarative APIs.

A *protocol layer* is a class that implements a single processing step in a
communication pipeline. Multiple layers are organized in a *protocol stack*.
Each layer may only communicate with its direct predecessor or successor in the
stack.

At the bottom of the protocol stack is usually a *transport layer*. For example,
an ``octet_stream::transport`` that manages a stream socket and provides access
to input and output buffers to the upper layer.

At the top of the protocol is an *application* that utilizes the lower layers
for communication via the network. Applications should only rely on the
*abstract interface type* when communicating with their lower layer. For
example, an application that processes a data stream should not implement
against a TCP socket interface directly. By programming against the abstract
interface types of ``caf.net``, users can instantiate an application with any
compatible protocol stack of their choosing. For example, a user may add extra
security by using application-level data encryption or combine a custom datagram
transport with protocol layers that establish ordering and reliability to
emulate a stream.

By default, ``caf.net`` distinguishes between these abstract interface types:

* *datagram*: A datagram interface provides access to some basic transfer units
  that may arrive out of order or not at all.
* *stream*: An octet stream interface represents a sequence of Bytes,
  transmitted reliable and in order.
* *message*: A message interface provides access to high-level, structured data.
  Messages usually consist of a header and a payload. A single message may span
  multiple datagrams.

Note that each interface type also depends on the *direction*, i.e., whether
talking to the upper or lower level. Incoming data always travels the protocol
stack *up*. Outgoing data always travels the protocol stack *down*.

A protocol stack always lives in a ``socket_manager``. The deepest layer in the
stack is always a ``socket_event_layer`` that simply turns events on sockets
(e.g., ready-to-read) into function calls. Only transport layers will implement
this layer.

A transport layer then responds to socket events by reading and writing to the
socket. The transport acts as the lower layer for the next layer in the
processing chain. For example, the ``octet_stream::transport`` is an
``octet_stream::lower_layer``. To interface with an octet stream, user-defined
classes implement ``octet_stream::upper_layer``.

When instantiating a protocol stack, each layer is represented by a concrete
object and we build the pipeline from top to bottom, i.e., we create the highest
layer first and then pass the last layer to the next lower layer until arriving
at the socket manager.

The layering API is generally structured into upper and lower layers. For
example, the upper layer for HTTP consumes the requests while the lower layer
can be used to send responses. Since the layering API is quite low level, we
recommend consulting the Doxygen documentation for the class interfaces and
looking at existing protocols such as the length-prefix framing as basis for
implementing custom protocols. In the manual, we focus on the high-level APIs.
