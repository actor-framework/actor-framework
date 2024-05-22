.. _message-passing:

Message Passing
===============

Sending and receiving messages is a fundamental concept in CAF. Actors
communicate by sending messages to each other. A message is a tuple of values
that can be delivered to local or remote actors (network transparency).

The class ``message`` holds a sequence of values of arbitrary types. Users can
create messages directly by calling ``make_message(...)`` or by using a
``message_builder``. The latter allows users to build messages incrementally.

However, users rarely interact with messages directly. Instead, they define
message handlers that process incoming messages and create message implicitly
when using the message passing API.

.. _copy-on-write:

Copy on Write
-------------

A ``message`` in CAF is a copy-on-write (COW) type (see
:ref:`copy-on-write-types`). This means that copying a message is cheap because
it only copies the reference to the message content. The actual copying of the
content only happens when one of the copies is modified.

This allows sending the same message to multiple receivers without copying
overhead, as long as all receivers only read the content of the message.

Actors copy message contents whenever other actors hold references to it and if
one or more arguments of a message handler take a mutable reference. Again, this
is transparent to the user most of the time. However, it is still important to
know about the COW semantics for understanding the performance characteristics
of an actor system.

.. _mail-api:

Sending Messages: The Mail API
------------------------------

Sending messages in CAF is starts by calling the member function ``mail`` or the
free function ``anon_mail``. The arguments of these functions are the contents
of the message. Then, CAF returns a builder object that allows users to specify
additional information about the message, such as the priority or whether the
message shall be send after some delay:

- Calling ``urgent()`` on the builder object sets the priority of the message to
  ``message_priority::high``. This causes the receiver to process the message
  before regular messages.
- Calling ``schedule(actor_clock::time_point timeout)`` on the builder object
  instructs CAF to send the message at a specific point in time.
- Calling ``delay(actor_clock::duration timeout)`` on the builder object
  instructs CAF to send the message after a relative timeout has passed.

In all cases, CAF returns a new builder object that allows users to chain
multiple calls. The final call to the builder object is one of:

- ``send(Handle receiver)`` to send the message to a specific actor as an
  asynchronous message (fire-and-forget).
- ``delegate(Handle receiver)`` to send the message to a specific actor and
  transfer the responsibility for responding to the original sender (see
  :ref:`delegate`).
- ``request(Handle receiver, timespan timeout)``
  to send the message to a specific actor as a request message. CAF will raise
  an error if the receiver does not respond within the specified timeout
  (passing ``infinite`` as timeout disables the timeout).

When sending a delayed or scheduled message, these member functions have two
additional, optional parameters: a ``RefTag`` and a ``SelfRefTag``. These tags
configure what kind of reference CAF will hold onto for the actors while the
message is pending. The ``RefTag`` specifies what kind of reference CAF will
hold onto for the receiver of the message. By default, CAF uses ``strong_ref``,
which instructs CAF to store a strong reference to the receiver. Passing
``weak_ref`` instead will cause CAF to store a weak reference instead. Likewise,
``SelfRefTag`` specifies what kind of reference CAF will hold onto for the
sender of the message. By default, CAF uses ``strong_self_ref``, which instructs
CAF to store a strong reference to the sender. Passing ``weak_self_ref`` will
cause CAF to store a weak reference instead.

When using weak references, CAF will try to convert them to strong references
before sending the message. If the conversion fails, CAF will drop the message.

When calling ``send`` or ``delegate``, the result is either ``void`` (immediate
send) or a ``disposable`` (delayed or scheduled send). The latter allows users
to cancel a send operation while it is still pending.

When calling ``request``, CAF will return a handle for the response message.
This handle either offers ``then`` and ``await`` member functions when using an
event-based actor or ``receive`` when using a blocking actor (see
:ref:`request`)

Note: the builder object from ``anon_send`` only supports ``send``.

Requirements for Message Types
------------------------------

Message types in CAF must meet the following requirements:

1. Inspectable (see :ref:`type-inspection`)
2. Default constructible
3. Copy constructible

A type ``T`` is inspectable if it provides a free function
``inspect(Inspector&, T&)`` or specializes ``inspector_access``.
Requirement 2 is a consequence of requirement 1, because CAF needs to be able to
create an object for ``T`` when deserializing incoming messages. Requirement 3
allows CAF to implement Copy on Write (see :ref:`copy-on-write`).

.. _special-handler:

.. _request:

Requests
--------

A main feature of CAF is its ability to couple input and output types via the
type system. For example, a ``typed_actor<result<int32_t>(int32_t)>``
essentially behaves like a function. It receives a single ``int32_t`` as input
and responds with another ``int32_t``. CAF embraces this functional take on
actors by simply creating response messages from the result of message handlers.
This allows CAF to match *request* to *response* messages and to provide a
convenient API for this style of communication.

.. _handling-response:

Sending Requests and Handling Responses
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Actors send request messages by calling
``mail(content...).request(receiver, timeout)``. This function returns an
intermediate object that allows an actor to set a one-shot handler for the
response message. Event-based actors can use either ``request(...).then`` or
``request(...).await``. The former multiplexes the one-shot handler with the
regular actor behavior and handles requests as they arrive. The latter suspends
the regular actor behavior until all awaited responses arrive and handles
requests in LIFO order. Blocking actors always use ``request(...).receive``,
which blocks until the one-shot handler was called. Actors receive a
``sec::request_timeout`` (see :ref:`sec`) error message (see
:ref:`error-message`) if a timeout occurs. Users can set the timeout to
``infinite`` for unbound operations. This is only recommended if the receiver is
known to run locally.

In our following example, we use the simple cell actor shown below as
communication endpoint.

.. literalinclude:: /examples/message_passing/request.cpp
   :language: C++
   :start-after: --(rst-cell-begin)--
   :end-before: --(rst-cell-end)--

To showcase the slight differences in API and processing order, we implement
three testee actors that all operate on a list of cell actors.

.. literalinclude:: /examples/message_passing/request.cpp
   :language: C++
   :start-after: --(rst-testees-begin)--
   :end-before: --(rst-testees-end)--

Our ``caf_main`` for the examples spawns five cells and assign the initial
values 0, 1, 4, 9, and 16. Then it spawns one instance for each of our testee
implementations and we can observe the different outputs.

Our ``waiting_testee`` actor will always print:

.. code-block:: none

   cell #9 -> 16
   cell #8 -> 9
   cell #7 -> 4
   cell #6 -> 1
   cell #5 -> 0

This is because ``await`` puts the one-shots handlers onto a stack and
enforces LIFO order by re-ordering incoming response messages as necessary.

The ``multiplexed_testee`` implementation does not print its results in
a predicable order. Response messages arrive in arbitrary order and are handled
immediately.

Finally, the ``blocking_testee`` has a deterministic output again. This is
because it blocks on each request until receiving the result before making the
next request.

.. code-block:: none

   cell #5 -> 0
   cell #6 -> 1
   cell #7 -> 4
   cell #8 -> 9
   cell #9 -> 16

Both event-based approaches send all requests, install a series of one-shot
handlers, and then return from the implementing function. In contrast, the
blocking function waits for a response before sending another request.

Error Handling in Requests
~~~~~~~~~~~~~~~~~~~~~~~~~~

Requests allow CAF to unambiguously correlate request and response messages.
This is also true if the response is an error message. Hence, CAF allows to add
an error handler as optional second parameter to ``then`` and ``await`` (this
parameter is mandatory for ``receive``). If no such handler is defined, the
default error handler (see :ref:`error-message`) is used as a fallback in
scheduled actors.

As an example, we consider a simple divider that returns an error on a division
by zero. This examples uses a custom error category (see :ref:`error`).

.. literalinclude:: /examples/message_passing/divider.cpp
   :language: C++
   :start-after: --(rst-divider-begin)--
   :end-before: --(rst-divider-end)--

When sending requests to the divider, we can use a custom error handlers to
report errors to the user like this:

.. literalinclude:: /examples/message_passing/divider.cpp
   :language: C++
   :start-after: --(rst-request-begin)--
   :end-before: --(rst-request-end)--

.. _delegate:

Delegating Messages
-------------------

Actors can transfer responsibility for a request by using ``delegate``.
This enables the receiver of the delegated message to reply as usual---simply
by returning a value from its message handler---and the original sender of the
message will receive the response. The following diagram illustrates request
delegation from actor B to actor C.

.. code-block:: none

                  A                  B                  C
                  |                  |                  |
                  | ---(request)---> |                  |
                  |                  | ---(delegate)--> |
                  |                  X                  |---\
                  |                                     |   | compute
                  |                                     |   | result
                  |                                     |<--/
                  | <-------------(reply)-------------- |
                  |                                     X
                  X

Returning the result of ``delegate(...)`` from a message handler, as
shown in the example below, suppresses the implicit response message and allows
the compiler to check the result type when using statically typed actors.

.. literalinclude:: /examples/message_passing/delegating.cpp
   :language: C++
   :start-after: --(rst-delegate-begin)--
   :end-before: --(rst-delegate-end)--

.. _promise:

Response Promises
-----------------

Response promises allow an actor to send and receive other messages prior to
replying to a particular request. Actors create a response promise using
``self->make_response_promise<Ts...>()``, where ``Ts`` is a
template parameter pack describing the promised return type. Dynamically typed
actors simply call ``self->make_response_promise()``. After retrieving
a promise, an actor can fulfill it by calling the member function
``deliver(...)``, as shown in the following example.

.. literalinclude:: /examples/message_passing/promises.cpp
   :language: C++
   :start-after: --(rst-promise-begin)--
   :end-before: --(rst-promise-end)--

This example is almost identical to the example for delegating messages.
However, there is a big difference in the flow of messages. In our first
version, the worker (C) directly responded to the client (A). This time, the
worker sends the result to the server (B), which then fulfills the promise and
thereby sends the result to the client:

.. code-block:: none

                  A                  B                  C
                  |                  |                  |
                  | ---(request)---> |                  |
                  |                  | ---(request)---> |
                  |                  |                  |---\
                  |                  |                  |   | compute
                  |                  |                  |   | result
                  |                  |                  |<--/
                  |                  | <----(reply)---- |
                  |                  |                  X
                  | <----(reply)---- |
                  |                  X
                  X

Special Message Handlers
------------------------

CAF has a few system-level message types (``exit_msg``, and ``error``) that all
actors should handle regardless of their current state. Consequently,
event-based actors handle such messages in special-purpose message handlers.
Note that blocking actors have neither of those special-purpose handlers (see
:ref:`blocking-actor`) and should instead provide callbacks for these messages
when calling ``receive``.

.. _exit-message:

Exit Handler
~~~~~~~~~~~~

Bidirectional monitoring with a strong lifetime coupling is established by
calling ``self->link_to(other)``. This will cause the runtime to send an
``exit_msg`` if either ``this`` or ``other`` dies. Per default, actors terminate
after receiving an ``exit_msg`` unless the exit reason is
``exit_reason::normal``. This mechanism propagates failure states in an actor
system. Linked actors form a sub system in which an error causes all actors to
fail collectively. Actors can override the default handler via
``set_exit_handler(f)``, where ``f`` is a function object with signature
``void (exit_message&)`` or ``void (scheduled_actor*, exit_message&)``.

.. _error-message:

Error Handler
~~~~~~~~~~~~~

Actors send error messages to others by returning an ``error`` (see
:ref:`error`) from a message handler. Similar to exit messages, error messages
usually cause the receiving actor to terminate, unless a custom handler was
installed via ``set_error_handler(f)``, where ``f`` is a function object with
signature ``void (error&)`` or ``void (scheduled_actor*, error&)``.
Additionally, ``request`` accepts an error handler as second argument to handle
errors for a particular request (see :ref:`error-response`). The default handler
is used as fallback if ``request`` is used without error handler.
