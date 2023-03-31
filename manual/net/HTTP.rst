.. include:: vars.rst

.. _net-http:

HTTP :sup:`experimental`
========================

Servers
-------

.. note::

  For this API, include ``caf/net/http/with.hpp``.

HTTP is an essential protocol for the world wide web as well as for micro
services. In CAF, starting an HTTP server with the declarative API uses the
entry point ``caf::net::http::with`` that takes an ``actor_system`` as argument.

On the result factory object, we can optionally call ``context`` to set an SSL
context for using HTTPS instead of plain HTTP connections. Once we call
``accept``, we enter the second phase of the setup. The ``accept`` function has
multiple overloads:

- |accept-port|
- |accept-socket|
- |accept-ssl-conn|

After calling ``accept``, we enter the second step of configuring the server.
Here, we can (optionally) fine-tune the server with these member functions:

- |do-on-error|
- |max-conn|
- |reuse-addr|
- |monitor|

At this step, we may also defines *routes* on the HTTP server. A route binds a
callback to an HTTP path on the server. On each HTTP request, the server
iterates over all routes and selects the first matching route to process the
request.

When defining a route, we pass an absolute path on the server, optionally the
HTTP method for the route and the handler. In the path, we can use ``<arg>``
placeholders. Each argument defined in this way maps to an argument of the
callback. The callback always must take ``http::responder&`` as the first
argument, followed by one argument for each ``<arg>`` placeholder in the path.

For example, the following route would forward any HTTP request on
``/user/<arg>`` with the HTTP method GET to the custom handler:

.. code-block:: C++

  .route("/user/<arg>", http::method::get, [](http::responder& res, int id) {
    // ...
  })

CAF evaluates the signature of the callback to automatically deduce the argument
types. On a mismatch, for example if a user accesses ``/user/foo``, the
conversion to ``int`` would fail and CAF would refuse the request with an error.

The ``responder`` encapsulates the state for responding to an HTTP request (see
`Responders`_) and allows our handler to send a response message either
immediately or at some later time.

To start an HTTP server, we have two overloads for ``start`` available.

The first ``start`` overload takes no arguments. Use this overload after
configuring at least one ``route`` to start an HTTP server that only dispatches
to its predefined routes, as shown in the example below. Calling this overload
without defining at least one route prior is an error.

.. literalinclude:: /examples/http/time-server.cpp
   :language: C++
   :start-after: --(rst-main-begin)--
   :end-before: --(rst-main-end)--

.. note::

  For details on ``ssl::context::enable``, please see :ref:`net-ssl`.

The second ``start`` overload takes one argument: a function object that takes
an asynchronous resource for processing ``http::request`` objects. This class
also allows to respond to HTTP requests, but is always asynchronous. Internally,
the class ``http::request`` is connected to the HTTP server with a future.

|after-start|

Responders
----------

The responder holds a pointer back to the HTTP server as well as pointers to the
HTTP headers and the payload.

The most commonly used member functions are as follows:

- ``const request_header& header() const noexcept`` to access the HTTP header
  fields.
- ``const_byte_span payload() const noexcept`` to access the bytes of the HTTP
  body (payload).
- ``actor_shell* self()`` for allowing the responder to interface with regular
  actors. See :ref:`net-actor-shell`.
- ``void respond(...)`` (multiple overloads) for sending a response message to
  the client.

The class also supports conversion to an asynchronous ``http::request`` via
``to_request`` or to create a promise on the server via ``to_promise``. The
promise is bound to the server and may not be used in another thread. Usually,
the promises are used in conjunction with ``self()->request(...)`` to generate
an HTTP response from an actor's response. Please look at the example under
``examples/http/rest.cpp`` as a reference.

|see-doxygen|
