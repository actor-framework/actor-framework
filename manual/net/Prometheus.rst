.. _net_prometheus:

Prometheus :sup:`experimental`
==============================

CAF ships a Prometheus server implementation that allows a scraper to collect
metrics from the actor system. The Prometheus server sits on top of an HTTP
layer.

Usually, users  can simply use the configuration options of the actor system to
export metrics to scrapers: :ref:`metrics_export`. When setting these options,
CAF uses this implementation to start the Prometheus server in the background.

Starting a Prometheus Server
----------------------------

The Prometheus server always binds to the actor system, because it needs the
multiplexer and the metrics registry. Hence, the entry point is always:

.. code-block:: C++

  caf::net::prometheus::with(sys)

From here, users can call ``accept`` with:

- A port, a bind address (defaults to the empty string for *any host*) and
  whether to create the socket with ``SO_REUSEADDR`` (defaults to ``true``).
- An ``ssl::context``, a port, a bind address and whether to create the socket
  with ``SO_REUSEADDR`` (with the same defaults as before).
- A ``tcp_accept_socket`` for accepting incoming connections.
- An ``ssl::acceptor`` for accepting incoming connections.

After setting up the factory, users may call these functions on the object:

- ``max_connections(size_t)`` for limiting the amount of concurrent connections
  on the server.
- ``do_on_error(function<void(const caf::error&)>)`` for setting an error
  callback.
- ``start()`` for initializing the server and running it in the background.
