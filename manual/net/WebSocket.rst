.. include:: vars.rst

.. _net-web-socket:

WebSocket :sup:`experimental`
=============================

Servers
-------

There are two ways to implement a WebSocket server with CAF. The first option is
to create a standalone server that only accepts incoming WebSocket clients. The
second option is to start a regular HTTP server and then use the
``caf::net::web_socket::switch_protocol`` factory to create a route for
WebSocket clients.

In both cases, the server runs in the background and communicates asynchronously
with the application logic over asynchronous buffer resource. Usually, this
resource is used to create an observable on some user-defined actor that
implements the server logic. The server passes connection events over this
buffer, whereas each event then consists of two new buffer resources: one for
receiving and one for sending messages from/to the WebSocket client.

.. note::

  Closing either of the asynchronous resources will close the WebSocket
  connection. When only reading from a WebSocket connection, we can subscribe
  the output channel to a ``never`` observable. Likewise, we can pass
  ``std::ignore`` to the input observable for applications that are only
  interested in writing to the WebSocket connection.

Standalone Server
~~~~~~~~~~~~~~~~~

.. note::

  For this API, include ``caf/net/web_socket/with.hpp``.

Starting a WebSocket server has three distinct steps.

In the first step, we bind the server to an actor system and optionally
configure SSL. The entry point is always calling the free function
``caf::net::web_socket::with`` that takes an ``actor_system`` as argument.
Optionally, users may set the ``Trait`` template parameter (see `Traits`_) of
the function. When not setting this parameter, it defaults to
``caf::net::web_socket::default_trait``. With this policy, the WebSocket sends
and receives ``caf::net::web_socket::frame`` objects (see `Frames`_).

On the result factory object, we can optionally call ``context`` to set an SSL
context. Once we call ``accept``, we enter the second phase of the setup.
The ``accept`` function has multiple overloads:

- |accept-port|
- |accept-socket|
- |accept-ssl-conn|

After calling ``accept``, we enter the second step of configuring the server.
Here, we can (optionally) fine-tune the server with these member functions:

- |do-on-error|
- |max-conn|
- |reuse-addr|

The second step is completed when calling ``on_request``. This function require
one argument: a function object that takes a
``net::web_socket::acceptor<Ts...>&`` argument, whereas the template parameter
pack ``Ts...`` is freely chosen and may be empty. The types we select here allow
us to pass any number of arguments to the connection event later on.

The third and final step is to call ``start`` with a function object that takes
an asynchronous resource for processing connect events. The type is usually
obtained from the configured trait class via
``Trait::acceptor_resource<Ts...>``, whereas ``Ts...`` must be the same set of
types we have used previously for the ``on_request`` handler.

|after-start|

This example illustrates how the API looks when putting all the pieces together:

.. literalinclude:: /examples/web_socket/echo.cpp
   :language: C++
   :start-after: --(rst-main-begin)--
   :end-before: --(rst-main-end)--

.. note::

  For details on ``ssl::context::enable``, please see :ref:`net-ssl`.

HTTP Server with a WebSocket Route
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. note::

  For this API, include ``caf/net/web_socket/switch_protocol.hpp``.

Sometimes, we want a server to accept WebSocket connections only on a specific
path and otherwise act as regular HTTP server. This use case is covered by
``caf::net::web_socket::switch_protocol``. The function is similar to ``with``
in that it takes a template parameter for the trait, but it requires no
arguments. In this version, we skip the ``accept`` step (since the HTTP server
takes care of accepting connections) and call ``on_request`` as the first step.
Then, we call ``on_start`` (instead of ``start``) to add the WebSocket server
logic when starting up the parent server.

The following snippet showcases the setup for adding a WebSocket route to an
HTTP server (you can find the full example under
``examples/web_socket/quote-server.cpp``):

.. literalinclude:: /examples/web_socket/quote-server.cpp
   :language: C++
   :start-after: --(rst-switch_protocol-begin)--
   :end-before: --(rst-switch_protocol-end)--

Clients
-------

Clients use the same entry point as standalone servers, i.e.,
``caf::net::web_socket::with``. However, instead of calling ``accept``, we call
``connect``. Like ``accept``, CAF offers multiple overloads for this function:

- |connect-host|
- |connect-uri|
- |connect-maybe-uri|
- |connect-socket|
- |connect-ssl|

.. note::

  When configuring an SSL context prior to calling ``connect``, the URI scheme
  *must* be ``wss``. When not configuring an SSL prior to calling ``connect``
  and passing an URI with scheme ``wss``, CAF will automatically create a SSL
  context for the connection.

After calling connect, we can configure various parameters:

- |do-on-error|
- |retry-delay|
- |conn-timeout|
- |max-retry|

Finally, we call ``start`` to launch the client. The function takes a function
object that must take two parameters: the input and output resources for reading
from and writing to the WebSocket connection. The types may be obtained from the
trait class (``caf::net::web_socket::default_trait`` unless passing a different
type to ``with``) via ``input_resource`` and ``output_resource``. However, the
function object (lambda) may also take the two parameters as ``auto`` for
brevity, as shown in the example below.

.. literalinclude:: /examples/web_socket/hello-client.cpp
   :language: C++
   :start-after: --(rst-main-begin)--
   :end-before: --(rst-main-end)--

Frames
------

The WebSocket protocol operates on so-called *frames*. A frame contains a single
text or binary message. The class ``caf::net::web_socket::frame`` is an
implicitly-shared handle type that represents a single WebSocket frame (binary
or text).

The most commonly used member functions are as follows:

- ``size_t size() const noexcept`` returns the size of the frame in bytes.
- ``bool empty() const noexcept`` queries whether the frame has a size of 0.
- ``bool is_binary() const noexcept`` queries whether the frame contains raw
  Bytes.
- ``bool is_text() const noexcept`` queries whether the frame contains a text
  message.
- ``const_byte_span as_binary() const noexcept`` accesses the bytes of a binary
  message.
- ``std::string_view as_text() const noexcept`` accesses the characters of a
  text message.

For the full class interface, please refer to the Doxygen documentation.

Traits
------

A trait translates between text or binary frames on the network and the
application by defining C++ types for reading and writing from/to the WebSocket
connection. The trait class also binds these types to the asynchronous resources
that connect the WebSocket in the background to the application logic.

The interface of the default looks as follows:

.. literalinclude:: /libcaf_net/caf/net/web_socket/default_trait.hpp
   :language: C++
   :start-after: --(rst-class-begin)--
   :end-before: --(rst-class-end)--

Users may implement custom trait types by providing the same member functions
and type aliases.
