.. _middleman:

Middleman
=========

The middleman is the main component of the I/O module and enables distribution.
It transparently manages proxy actor instances representing remote actors,
maintains connections to other nodes, and takes care of serialization of
messages. Applications install a middleman by loading ``caf::io::middleman`` as
module (see :ref:`system-config`). Users can include ``"caf/io/all.hpp"`` to get
access to all public classes of the I/O module.

Class ``middleman``
-------------------

+---------------------------------------------------------------+----------------------+
| **Remoting**                                                  |                      |
+---------------------------------------------------------------+----------------------+
| ``expected<uint16> open(uint16, const char*, bool)``          | See :ref:`remoting`. |
+---------------------------------------------------------------+----------------------+
| ``expected<uint16> publish(T, uint16, const char*, bool)``    | See :ref:`remoting`. |
+---------------------------------------------------------------+----------------------+
| ``expected<void> unpublish(T x, uint16)``                     | See :ref:`remoting`. |
+---------------------------------------------------------------+----------------------+
| ``expected<node_id> connect(std::string host, uint16_t port)``| See :ref:`remoting`. |
+---------------------------------------------------------------+----------------------+
| ``expected<T> remote_actor<T = actor>(string, uint16)``       | See :ref:`remoting`. |
+---------------------------------------------------------------+----------------------+
| ``expected<T> spawn_broker(F fun, ...)``                      | See :ref:`broker`.   |
+---------------------------------------------------------------+----------------------+
| ``expected<T> spawn_client(F, string, uint16, ...)``          | See :ref:`broker`.   |
+---------------------------------------------------------------+----------------------+
| ``expected<T> spawn_server(F, uint16, ...)``                  | See :ref:`broker`.   |
+---------------------------------------------------------------+----------------------+

.. _remoting:

Publishing and Connecting
-------------------------

The member function ``publish`` binds an actor to a given port, thereby
allowing other nodes to access it over the network.

.. code-block:: C++

   template <class T>
   expected<uint16_t> middleman::publish(T x, uint16_t port,
                                         const char* in = nullptr,
                                         bool reuse_addr = false);

The first argument is a handle of type ``actor`` or
``typed_actor<...>``. The second argument denotes the TCP port. The OS
will pick a random high-level port when passing 0. The third parameter
configures the listening address. Passing null will accept all incoming
connections (``INADDR_ANY``). Finally, the flag ``reuse_addr``
controls the behavior when binding an IP address to a port, with the same
semantics as the BSD socket flag ``SO_REUSEADDR``. For example, with
``reuse_addr = false``, binding two sockets to 0.0.0.0:42 and
10.0.0.1:42 will fail with ``EADDRINUSE`` since 0.0.0.0 includes 10.0.0.1.
With ``reuse_addr = true`` binding would succeed because 10.0.0.1 and
0.0.0.0 are not literally equal addresses.

The member function returns the bound port on success. Otherwise, an ``error``
(see :ref:`error`) is returned.

.. code-block:: C++

   template <class T>
   expected<uint16_t> middleman::unpublish(T x, uint16_t port = 0);

The member function ``unpublish`` allows actors to close a port
manually. This is performed automatically if the published actor terminates.
Passing 0 as second argument closes all ports an actor is published to,
otherwise only one specific port is closed.

The function returns an ``error`` (see :ref:`error`) if the actor was not bound
to given port.

.. code-block:: C++

   template<class T = actor>
   expected<T> middleman::remote_actor(std::string host, uint16_t port);

After a server has published an actor with ``publish``, clients can
connect to the published actor by calling ``remote_actor``:

.. code-block:: C++

   // node A
   auto ping = spawn(ping);
   system.middleman().publish(ping, 4242);

   // node B
   auto ping = system.middleman().remote_actor("node A", 4242);
   if (! ping) {
     cerr << "unable to connect to node A: "
          << system.render(ping.error()) << std::endl;
   } else {
     self->send(*ping, ping_atom::value);
   }

There is no difference between server and client after the connection phase.
Remote actors use the same handle types as local actors and are thus fully
transparent.

The function pair ``open`` and ``connect`` allows users to connect CAF instances
without remote actor setup. The function ``connect`` returns a ``node_id`` that
can be used for remote spawning (see (see :ref:`remote-spawn`)).

.. _free-remoting-functions:

Free Functions
--------------

The following free functions in the namespace ``caf::io`` avoid calling
the middleman directly. This enables users to easily switch between
communication backends as long as the interfaces have the same signatures. For
example, the (experimental) OpenSSL binding of CAF implements the same
functions in the namespace ``caf::openssl`` to easily switch between
encrypted and unencrypted communication.

+------------------------------------------------------------------------------+----------------------+
| ``expected<uint16> open(actor_system&, uint16, const char*, bool)``          | See :ref:`remoting`. |
+------------------------------------------------------------------------------+----------------------+
| ``expected<uint16> publish(T, uint16, const char*, bool)``                   | See :ref:`remoting`. |
+------------------------------------------------------------------------------+----------------------+
| ``expected<void> unpublish(T x, uint16)``                                    | See :ref:`remoting`. |
+------------------------------------------------------------------------------+----------------------+
| ``expected<node_id> connect(actor_system&, std::string host, uint16_t port)``| See :ref:`remoting`. |
+------------------------------------------------------------------------------+----------------------+
| ``expected<T> remote_actor<T = actor>(actor_system&, string, uint16)``       | See :ref:`remoting`. |
+------------------------------------------------------------------------------+----------------------+

.. _transport-protocols:

Transport Protocols  :sup:`experimental`
----------------------------------------

CAF communication uses TCP per default and thus the functions shown in the
middleman API above are related to TCP. There are two alternatives to plain TCP:
TLS via the OpenSSL module shortly discussed in (see
:ref:`free-remoting-functions`) and UDP.

UDP is integrated in the default multiplexer and BASP broker. Set the flag
``middleman_enable_udp`` to true to enable it (see :ref:`system-config`). This
does not require you to disable TCP. Use ``publish_udp`` and
``remote_actor_udp`` to establish communication.

Communication via UDP is inherently unreliable and unordered. CAF reestablishes
order and drops messages that arrive late. Messages that are sent via datagrams
are limited to a maximum of 65.535 bytes which is used as a receive buffer size
by CAF. Note that messages that exceed the MTU are fragmented by IP and are
considered lost if a single fragment is lost. Optional reliability based on
retransmissions and messages slicing on the application layer are planned for
the future.
