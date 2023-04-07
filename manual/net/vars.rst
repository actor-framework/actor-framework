.. |accept-port| replace:: \
   ``accept(uint16_t port, std::string bind_address = "")`` for opening a new \
   ``port``. The ``bind_address`` optionally restricts which IP addresses may \
   connect to the server. Passing an empty string (default) allows any client.

.. |accept-socket| replace:: \
   ``accept(tcp_accept_socket fd)`` for running the server on already \
   configured TCP server socket.

.. |accept-ssl-conn| replace:: \
   ``accept(ssl::tcp_acceptor acc)`` for running the server on already \
   configured TCP server socket with an SSL context. Using this overload \
   after configuring an SSL context is an error, since CAF cannot use two SSL \
   contexts on one server.

.. |connect-host| replace:: \
   ``connect(std::string host, uint16_t port)`` for connecting to the server \
   at ``host`` on the given ``port``.

.. |connect-uri| replace:: \
   ``connect(uri endpoint)`` for connecting to the given server. The URI \
   scheme must be either ``ws`` or ``wss``.

.. |connect-maybe-uri| replace:: \
   ``connect(expected<uri> endpoint)`` like the previous overload. Forwards \
   the error in case ``endpoint`` does not represent an actual value. Useful \
   for passing the result of ``make_uri`` to ``connect`` directly without \
   having to check the result first.

.. |connect-socket| replace:: \
   ``connect(stream_socket fd)`` for establishing a WebSocket connection on an \
   already connected TCP socket.

.. |connect-ssl| replace:: \
   ``connect(ssl::tcp_acceptor acc)`` for establishing a WebSocket connection \
   on an already established SSL connection.

.. |do-on-error| replace:: \
   ``do_on_error(F callback)`` installs a callback function that CAF calls if \
   an error occurs while starting the server.

.. |max-conn| replace:: \
   ``max_connections(size_t value)`` configures how many clients the server \
   may allow to connect before blocking further connections attempts.

.. |reuse-addr| replace:: \
   ``reuse_address(bool value)`` configures whether we create the server \
   socket with ``SO_REUSEADDR``. Has no effect if we have passed a socket or \
   SSL acceptor to ``accept``.

.. |monitor| replace:: \
   ``monitor(ActorHandle hdl)`` configures the server to monitor an actor and \
   automatically stop the server when the actor terminates.

.. |retry-delay| replace:: \
   ``retry_delay(timespan)`` to set the delay between connection retries when \
   a connection attempt fails.

.. |conn-timeout| replace:: \
   ``connection_timeout(timespan)`` to set the maximum amount of time to wait \
   for a connection attempt to succeed before giving up.

.. |max-retry| replace:: \
   ``max_retry_count(size_t)`` to set the maximum number of times to retry a \
   connection attempt before giving up.

.. |after-start| replace:: \
   After calling ``start``, CAF returns an ``expected<disposble>``. On \
   success, the ``disposable`` is a handle to the launched server and calling \
   ``dispose`` on it stops the server. On error, the result contains a \
   human-readable description of what went wrong.

.. |see-doxygen| replace:: \
   For the full class interface, please refer to the \
   `Doxygen documentation <https://www.actor-framework.org/doxygen>`__.
