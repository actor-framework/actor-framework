Concepts
========

Before diving into the API of CAF, we discuss the concepts behind it and
explain the terminology used in this manual.

Actor Model
-----------

The actor model describes concurrent entities---actors---that do not share
state and communicate only via asynchronous message passing. Decoupling
concurrently running software components via message passing avoids race
conditions by design. Actors can create---spawn---new actors and monitor each
other to build fault-tolerant, hierarchical systems. Since message passing is
network transparent, the actor model applies to both concurrency and
distribution.

Implementing applications on top of low-level primitives such as mutexes and
semaphores has proven challenging and error-prone. In particular when trying to
implement applications that scale up to many CPU cores. Queueing, starvation,
priority inversion, and false sharing are only a few of the issues that can
decrease performance significantly in mutex-based concurrency models. In the
extreme, an application written with the standard toolkit can run slower when
adding more cores.

The actor model has gained momentum over the last decades due to its high level
of abstraction and its ability to scale dynamically from one core to many cores
and from one node to many nodes. However, the actor model has not yet been
widely adopted in the native programming domain. With CAF, we contribute a
framework for actor programming in C++ as open-source software to ease native
development of concurrent as well as distributed systems. In this regard, CAF
follows the C++ philosophy *building the highest abstraction possible
without sacrificing performance*.

Terminology
-----------

CAF is inspired by other implementations based on the actor model such as
Erlang or Akka. It aims to provide a modern C++ API allowing for type-safe as
well as dynamically typed messaging. While there are similarities to other
implementations, we made many different design decisions that lead to slight
differences when comparing CAF to other actor frameworks.

Dynamically Typed Actor
~~~~~~~~~~~~~~~~~~~~~~~

A dynamically typed actor accepts any kind of message and dispatches on its
content dynamically at the receiver. This is the traditional messaging style
found in implementations like Erlang or Akka. The upside of this approach is
(usually) faster prototyping and less code. This comes at the cost of requiring
excessive testing.

Statically Typed Actor
~~~~~~~~~~~~~~~~~~~~~~

CAF achieves static type-checking for actors by defining abstract messaging
interfaces. Since interfaces define both input and output types, CAF is able to
verify messaging protocols statically. The upside of this approach is much
higher robustness to code changes and fewer possible runtime errors. This comes
at an increase in required source code, as developers have to define and use
messaging interfaces.

.. _actor-reference:

Actor References
~~~~~~~~~~~~~~~~

CAF uses reference counting for actors. The three ways to store a reference to
an actor are addresses, handles, and pointers. Note that *address* does
not refer to a *memory region* in this context.

.. _actor-address:

Address
+++++++

Each actor has a (network-wide) unique logical address. This identifier is
represented by ``actor_addr``, which allows to identify and monitor an actor.
Unlike other actor frameworks, CAF does *not* allow users to send messages to
addresses. This limitation is due to the fact that the address does not contain
any type information. Hence, it would not be safe to send it a message, because
the receiving actor might use a statically typed interface that does not accept
the given message. Because an ``actor_addr`` fills the role of an identifier, it
has *weak reference semantics* (see :ref:`reference-counting`).

.. _actor-handle:

Handle
++++++

An actor handle contains the address of an actor along with its type information
and is required for sending messages to actors. The distinction between handles
and addresses---which is unique to CAF when comparing it to other actor
systems---is a consequence of the design decision to enforce static type
checking for all messages. Dynamically typed actors use ``actor`` handles, while
statically typed actors use ``typed_actor<...>`` handles. Both types have
*strong reference semantics* (see :ref:`reference-counting`).

.. _actor-pointer:

Pointer
+++++++

In a few instances, CAF uses ``strong_actor_ptr`` to refer to an actor using
*strong reference semantics* (see :ref:`reference-counting`) without knowing the
proper handle type. Pointers must be converted to a handle via ``actor_cast``
(see :ref:`actor-cast`) prior to sending messages. A ``strong_actor_ptr`` can be
*null*.

Spawning
~~~~~~~~

*Spawning* an actor means to create and run a new actor.

.. _monitor:

Monitor
~~~~~~~

A monitor is a unidirectional connection where one actor observes the lifetime
of another actor. A monitored actor sends sends it exit reason to all actors
monitoring it as part of its termination. This allows actors to supervise other
actors and to take actions when one of the supervised actors fails, i.e.,
terminates with a non-normal exit reason.

.. _link:

Link
~~~~

A link is a bidirectional connection between two actors. Each actor sends an
exit message (see :ref:`exit-message`) to all of its links as part of its
termination. Unlike down messages, exit messages cause the receiving actor to
terminate as well when receiving a non-normal exit reason per default. This
allows developers to create a set of actors with the guarantee that either all
or no actors are alive. Actors can override the default handler to implement
error recovery strategies.

Experimental Features
---------------------

Sections that discuss experimental features are highlighted with
:sup:`experimental`. The API of such features is not stable. This means even
minor updates to CAF can come with breaking changes to the API or even remove a
feature completely. However, we encourage developers to extensively test such
features and to start discussions to uncover flaws, report bugs, or tweaking the
API in order to improve a feature or streamline it to cover certain use cases.
