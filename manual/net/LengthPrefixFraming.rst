.. include:: vars.rst

.. _net-lpf:

Length-prefix Framing :sup:`experimental`
=========================================

Length-prefix framing is a simple protocol for encoding variable-length messages
on the network. Each message is preceded by a fixed-length value (32 bit) that
indicates the length of the message in bytes.

When a sender wants to transmit a message, it first encodes the message into a
sequence of bytes. It then calculates the length of the byte sequence and
prefixes the message with the length. The receiver then reads the length prefix
to determine the length of the message before reading the bytes for the message.

.. note::

  For the high-level API, include ``caf/net/lp/with.hpp``.

Servers
-------

The simplest way to start a length-prefix framing server is by using the
high-level factory DSL. The entry point for this API is calling the
``caf::net::lp::with`` function:

.. code-block:: C++

    caf::net::lp::with(sys)

Optionally, you can also provide a custom trait type by calling
``caf::net::lp::with<my_trait>(sys)`` instead. The default trait class
``caf::net::lp::default_trait`` configures the transport to exchange
``caf::net::lp::frame`` objects with the application, which essentially wraps
raw bytes.

Once you have set up the factory, you can call the ``accept`` function on it to
start accepting incoming connections. The ``accept`` function has multiple
overloads:

- |accept-port|
- |accept-socket|
- |accept-ssl-conn|

After setting up the factory and calling ``accept``, you can use the following
functions on the returned object:

- |do-on-error|
- |max-conn|
- |reuse-addr|
- ``start(OnStart)`` to initialize the server and run it in the background. The
  ``OnStart`` callback takes an argument of type ``trait::acceptor_resource``.
  This is a consumer resource for receiving accept events. Each accept event
  consists of an input resource and an output resource for reading from and
  writing to the new connection.

|after-start|

Clients
-------

Starting a client also uses the ``with`` factory. Calling ``connect`` instead of
``accept`` creates a factory for clients. The ``connect`` function may be called
with:

- |connect-host|
- |connect-socket|
- |connect-ssl|

After calling connect, we can configure various parameters:

- |do-on-error|
- |retry-delay|
- |conn-timeout|
- |max-retry|

Finally, we call ``start`` to launch the client. The function expects an
``OnStart`` callback takes two arguments: the input resource and the output
resource for reading from and writing to the new connection.

Configuration
-------------

The length-prefix framing can be configured to use different size field types.
The default size field type is ``u4`` (4 bytes).

- ``u1``: 1-byte prefix (0-255 bytes)
- ``u2``: 2-byte prefix (0-64KB)
- ``u4``: 4-byte prefix (0-4GB)
- ``u8``: 8-byte prefix (0-16EB)

The size field type can be configured using the ``size_field`` option.

In addition, you can configure the maximum message length. The default maximum
message length is 64MB to prevent memory exhaustion attacks. Messages larger
than the size field type limit are automatically rejected. The maximum message
length can be configured using the ``max_message_size`` option.

.. note::

  Both client and server must use the same size field type for successful
  communication. Mismatched size field types will result in protocol errors.
