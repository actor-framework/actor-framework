.. _broker:

Network I/O with Brokers
========================

When communicating to other services in the network, sometimes low-level socket
I/O is inevitable. For this reason, CAF provides *brokers*. A broker is
an event-based actor running in the middleman that multiplexes socket I/O. It
can maintain any number of acceptors and connections. Since the broker runs in
the middleman, implementations should be careful to consume as little time as
possible in message handlers. Brokers should outsource any considerable amount
of work by spawning new actors or maintaining worker actors.

*Note that all UDP-related functionality is still  :sup:`experimental`.*

Spawning Brokers
----------------

Brokers are implemented as functions and are spawned by calling on of the three
following member functions of the middleman.

.. code-block:: C++

   template <spawn_options Os = no_spawn_options,
             class F = std::function<void(broker*)>, class... Ts>
   typename infer_handle_from_fun<F>::type
   spawn_broker(F fun, Ts&&... xs);

   template <spawn_options Os = no_spawn_options,
             class F = std::function<void(broker*)>, class... Ts>
   expected<typename infer_handle_from_fun<F>::type>
   spawn_client(F fun, const std::string& host, uint16_t port, Ts&&... xs);

   template <spawn_options Os = no_spawn_options,
             class F = std::function<void(broker*)>, class... Ts>
   expected<typename infer_handle_from_fun<F>::type>
   spawn_server(F fun, uint16_t port, Ts&&... xs);

The function ``spawn_broker`` simply spawns a broker. The convenience
function ``spawn_client`` tries to connect to given host and port over
TCP and returns a broker managing this connection on success. Finally,
``spawn_server`` opens a local TCP port and spawns a broker managing it
on success. There are no convenience functions spawn a UDP-based client or
server.

.. _broker-class:

Class ``broker``
----------------

.. code-block:: C++

   void configure_read(connection_handle hdl, receive_policy::config config);

Modifies the receive policy for the connection identified by ``hdl``.
This will cause the middleman to enqueue the next ``new_data_msg``
according to the given ``config`` created by
``receive_policy::exactly(x)``, ``receive_policy::at_most(x)``,
or ``receive_policy::at_least(x)`` (with ``x`` denoting the
number of bytes).

.. code-block:: C++

   void write(connection_handle hdl, size_t num_bytes, const void* buf)
   void write(datagram_handle hdl, size_t num_bytes, const void* buf)

Writes data to the output buffer.

.. code-block:: C++

   void enqueue_datagram(datagram_handle hdl, std::vector<char> buf);

Enqueues a buffer to be sent as a datagram. Use of this function is encouraged
over write as it allows reuse of the buffer which can be returned to the broker
in a ``datagram_sent_msg``.

.. code-block:: C++

   void flush(connection_handle hdl);
   void flush(datagram_handle hdl);

Sends the data from the output buffer.

.. code-block:: C++

   template <class F, class... Ts>
   actor fork(F fun, connection_handle hdl, Ts&&... xs);

Spawns a new broker that takes ownership of a given connection.

.. code-block:: C++

   size_t num_connections();

Returns the number of open connections.

.. code-block:: C++

   void close(connection_handle hdl);
   void close(accept_handle hdl);
   void close(datagram_handle hdl);

Closes the endpoint related to the handle.

.. code-block:: C++

   expected<std::pair<accept_handle, uint16_t>>
   add_tcp_doorman(uint16_t port = 0, const char* in = nullptr,
                   bool reuse_addr = false);

Creates new doorman that accepts incoming connections on a given port and
returns the handle to the doorman and the port in use or an error.

.. code-block:: C++

   expected<connection_handle>
   add_tcp_scribe(const std::string& host, uint16_t port);

Creates a new scribe to connect to host:port and returns handle to it or an
error.

.. code-block:: C++

   expected<std::pair<datagram_handle, uint16_t>>
   add_udp_datagram_servant(uint16_t port = 0, const char* in = nullptr,
                            bool reuse_addr = false);

Creates a datagram servant to handle incoming datagrams on a given port.
Returns the handle to the servant and the port in use or an error.

.. code-block:: C++

   expected<datagram_handle>
   add_udp_datagram_servant(const std::string& host, uint16_t port);

Creates a datagram servant to send datagrams to host:port and returns a handle
to it or an error.

Broker-related Message Types
----------------------------

Brokers receive system messages directly from the middleman for connection and
acceptor events.

**Note:** brokers are *required* to handle these messages immediately
regardless of their current state. Not handling the system messages from the
broker results in loss of data, because system messages are *not*
delivered through the mailbox and thus cannot be skipped.

.. code-block:: C++

   struct new_connection_msg {
     accept_handle source;
     connection_handle handle;
   };

Indicates that ``source`` accepted a new TCP connection identified by
``handle``.

.. code-block:: C++

   struct new_data_msg {
     connection_handle handle;
     std::vector<char> buf;
   };

Contains raw bytes received from ``handle``. The amount of data
received per event is controlled with ``configure_read`` (see
broker-class_). It is worth mentioning that the buffer is re-used whenever
possible.

.. code-block:: C++

   struct data_transferred_msg {
     connection_handle handle;
     uint64_t written;
     uint64_t remaining;
   };

This message informs the broker that the ``handle`` sent
``written`` bytes with ``remaining`` bytes in the buffer. Note,
that these messages are not sent per default but must be explicitly enabled via
the member function ``ack_writes``.

.. code-block:: C++

   struct connection_closed_msg {
     connection_handle handle;
   };

   struct acceptor_closed_msg {
     accept_handle handle;
   };

A ``connection_closed_msg`` or ``acceptor_closed_msg`` informs
the broker that one of its handles is no longer valid.

.. code-block:: C++

   struct connection_passivated_msg {
     connection_handle handle;
   };

   struct acceptor_passivated_msg {
     accept_handle handle;
   };

A ``connection_passivated_msg`` or ``acceptor_passivated_msg``
informs the broker that one of its handles entered passive mode and no longer
accepts new data or connections trigger_.

The following messages are related to UDP communication (see
:ref:`transport-protocols`). Since UDP is not connection oriented, there is no
equivalent to the ``new_connection_msg`` of TCP.

.. code-block:: C++

   struct new_datagram_msg {
     datagram_handle handle;
     network::receive_buffer buf;
   };

Contains the raw bytes from ``handle``. The buffer always has a maximum
size of 65k to receive all regular UDP messages. The amount of bytes can be
queried via the ``.size()`` member function. Similar to TCP, the buffer
is reused when possible---please do not resize it.

.. code-block:: C++

   struct datagram_sent_msg {
     datagram_handle handle;
     uint64_t written;
     std::vector<char> buf;
   };

This message informs the broker that the ``handle`` sent a datagram of
``written`` bytes. It includes the buffer that held the sent message to
allow its reuse. Note, that these messages are not sent per default but must be
explicitly enabled via the member function ``ack_writes``.

.. code-block:: C++

   struct datagram_servant_closed_msg {
     std::vector<datagram_handle> handles;
   };

A ``datagram_servant_closed_msg`` informs the broker that one of its
handles is no longer valid.

.. code-block:: C++

   struct datagram_servant_passivated_msg {
     datagram_handle handle;
   };

A ``datagram_servant_closed_msg`` informs the broker that one of its
handles entered passive mode and no longer accepts new data trigger_.

.. _trigger:

Manually Triggering Events  :sup:`experimental`
-----------------------------------------------

Brokers receive new events as ``new_connection_msg`` and
``new_data_msg`` as soon and as often as they occur, per default. This
means a fast peer can overwhelm a broker by sending it data faster than the
broker can process it. In particular if the broker outsources work items to
other actors, because work items can accumulate in the mailboxes of the
workers.

Calling ``self->trigger(x,y)``, where ``x`` is a connection or
acceptor handle and ``y`` is a positive integer, allows brokers to halt
activities after ``y`` additional events. Once a connection or acceptor
stops accepting new data or connections, the broker receives a
``connection_passivated_msg`` or ``acceptor_passivated_msg``.

Brokers can stop activities unconditionally with ``self->halt(x)`` and
resume activities unconditionally with ``self->trigger(x)``.
