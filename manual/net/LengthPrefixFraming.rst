.. _length_prefix_framing:

Length-prefix Framing :sup:`experimental`
=========================================

Length-prefix framing is a simple protocol for encoding variable-length messages
on the network. Each message is preceded by a fixed-length value (32 bit) that
indicates the length of the message, in bytes.

When a sender wants to transmit a message, it first encodes the message into a
sequence of bytes. It then calculates the length of the byte sequence and
prefixes the message with the length. The receiver then reads the length prefix
to determine the length of the message before reading the bytes for the message.

Starting a Server
-----------------

The simplest way to start a length-prefix framing server is by using the
high-level factory API.

First, include the necessary header files:

.. code-block:: cpp

    #include <caf/net/lp/with.hpp>

Then, create a new length-prefix factory using the ``caf::net::lp::with``
function:

.. code-block:: cpp

    caf::net::lp::with(sys)

Optionally, you can also provide a custom trait type by calling
``caf::net::lp::with<my_trait>(sys)`` instead.

Once you have set up the factory, you can call the ``accept`` function on it to
start accepting incoming connections. The ``accept`` function can be called
with:

- A port number, a bind address (which defaults to the empty string, allowing
  connections from any host), and a boolean indicating whether to create the
  socket with ``SO_REUSEADDR`` (which defaults to ``true``).
- An ``ssl::context`` object (if you want to use SSL/TLS encryption), a port
  number, a bind address, and a boolean indicating whether to create the socket
  with ``SO_REUSEADDR`` (with the same defaults as before).
- A ``tcp_accept_socket`` object for accepting incoming connections.
- An ``ssl::acceptor`` object for accepting incoming connections over SSL/TLS.

After setting up the factory and calling ``accept``, you can use the following
functions on the returned object:

- ``max_connections(size_t)`` to limit the maximum number of concurrent
  connections on the server.
- ``do_on_error(function<void(const caf::error&)>)`` to set an error callback.
- ``start(OnStart)`` to initialize the server and run it in the background. The
  ``OnStart`` callback takes an argument of type ``trait::acceptor_resource``.
  This is a consumer resource for receiving accept events. Each accept event
  consists of an input resource and an output resource for reading from and
  writing to the new connection.

Starting a Client
-----------------

Starting a client also uses the ``with`` factory. Calling ``connect`` instead of
``accept`` creates a factory for clients. The ``connect`` function may be called
with:

- A remote address (such as a hostname or IP address) plus a port number.
- An ``url`` object using the ``tcp`` schema, i.e., ``tcp://<host>:<port>``.
- A ``stream_socket`` object for running the length-prefix framing.
- An ``ssl::connection`` object for running the length-prefix framing.

After setting up the factory and calling ``connect``, you can use the following
functions on the returned object:

- ``do_on_error(function<void(const caf::error&)>)`` to set an error callback.
- ``start(OnStart)`` to initialize the server and run it in the background. The
- ``retry_delay(timespan)`` to set the delay between connection retries when a
  connection attempt fails.
- ``connection_timeout(timespan)`` to set the maximum amount of time to wait for
  a connection attempt to succeed before giving up.
- ``max_retry_count(size_t)`` to set the maximum number of times to retry a
  connection attempt before giving up.
- ``OnStart`` callback takes two arguments: the input resource and the output
  resource for reading from and writing to the new connection.

Note that ``retry_delay``, ``connection_timeout(timespan)`` and
``max_retry_count(size_t)`` have no effect when constructing the factory object
from a socket or SSL connection.
