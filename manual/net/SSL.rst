.. include:: vars.rst

.. _net-ssl:

SSL
===

Protocols such as HTTP and WebSocket have a *secure* version that requires an
SSL/TLS layer. CAF bundles functions and classes for implementing secure
communication in the namespace ``caf::net::ssl``. It is worth mentioning that
classes and functions in this namespace only provide convenient access to SSL
capabilities by wrapping an actual SSL implementation (usually OpenSSL).

Context
-------

CAF users usually only need to care about the class ``context``. This is the
central state of an SSL-enabled server or client. On a server, all incoming
connection will use the configuration from this ``context``.

To create a new context, we can use one of these factory member functions:

- ``enable(bool flag)``
- ``make(tls min_version, tls max_version = tls::any)``
- ``make_server(tls min_version, tls max_version = tls::any)``
- ``make_client(tls min_version, tls max_version = tls::any)``
- ``make(dtls min_version, dtls max_version = dtls::any)``
- ``make_server(dtls min_version, dtls max_version = dtls::any)``
- ``make_client(dtls min_version, dtls max_version = dtls::any)``

All ``make*`` functions return an ``caf::expected<caf::net::ssl::context>``.
However, the factory function ``enable`` returns ``caf::expected<void>``
instead. This function is meant as an entry point for creating a ``context``
through a series of chained ``and_then`` invocations on the ``expected``. Here
is an example to illustrate how it works in practice:

.. code-block:: C++

  namespace ws = caf::net::web_socket;
  auto pem = ssl::format::pem;
  auto key_file = caf::get_as<std::string>(cfg, "tls.key-file");
  auto cert_file = caf::get_as<std::string>(cfg, "tls.cert-file");
  if (!key_file != !cert_file) {
    std::cerr << "*** inconsistent TLS config: declare neither file or both\n";
    return EXIT_FAILURE;
  }
  auto server
    = ws::with(sys)
        .context(ssl::context::enable(key_file && cert_file)
                   .and_then(ssl::emplace_server(ssl::tls::v1_2))
                   .and_then(ssl::use_private_key_file(key_file, pem))
                   .and_then(ssl::use_certificate_file(cert_file, pem)))
        // ...

Passing ``false`` to ``ssl::context::enable`` returns an ``expected`` with a
default-constructed ``caf::error``. Since the ``expected`` contains an
``error``, all subsequent ``and_then`` calls turn into no-ops. However, a
default-constructed ``error`` means "no error". Hence, CAF continues without an
SSL context in the example and the server will use plain (unencrypted)
WebSocket communication.

Passing ``true`` to ``ssl::context::enable`` returns an "empty"
``expected<void>``. The next call to ``and_then`` will call the function object
(with no arguments) and return a new ``expected``. In the example, we use
``emplace_server`` to create a function object that will call
``context::make_server(min, max)`` when called without arguments. Hence, we
convert an ``expected<void>`` to an ``expected<ssl::context>`` with this step.
The functions ``ssl::use_private_key_file`` and ``ssl::use_certificate_file``
return functions objects (lambdas) that take an ``ssl::context`` as argument and
return ``expected<ssl::context>`` again. In this way, we can chain functions
that modify a ``context`` with ``and_then`` to describe the setup for building
up an SSL context in a declarative way. If all of the steps for building the SSL
context succeed, the server will use WebSocket over SSL to encrypt communication
to clients. If any of the steps produces an actual error (for example if the key
file does not exist), CAF will produce an error and not start the server at all.

|see-doxygen|
