.. _net_overview:

Overview
========

The networking module offers high-level APIs for individual protocols as well as
low-level building blocks for building implementing new protocols and assembling
protocol stacks.

High-level APIs
===============

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

In general, connecting to a server follows the pattern:

.. code-block:: C++

   PROTOCOL::with(CONTEXT).connect(WHERE).start(ON_CONNECT);

To establish the connection asynchronously:

.. code-block:: C++

   PROTOCOL::with(CONTEXT).async_connect(WHERE).start(ON_CONNECT);

And for accepting incoming connections:

.. code-block:: C++

   PROTOCOL::with(CONTEXT).accept(WHERE).start(ON_ACCEPT);

CAF also includes some self-contained services that users may simply start in
the background such as a Prometheus endpoint. These services take no callback in
``start``. For example:

.. code-block:: C++

   prometheus::with(sys).accept(8008).start();

In all cases, CAF returns a ``disposable`` that allows users to cancel the
activity at any time. Note: when canceling a server, it only disposes the accept
socket itself. Previously established connections remain unaffected.

For error reporting, most factories allow setting a callback with
``do_on_error``.

Many protocols also accept additional configuration parameters. For example, the
factory for establishing WebSocket connections allows users to tweak parameters
for the handshake such as protocols or extensions fields. These options are
listed at the documentation for the individual protocols.

Layering
========
