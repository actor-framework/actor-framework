.. _message-handler:

Message Handlers
================

Actors can store a set of callbacks---usually implemented as lambda
expressions---using either ``behavior`` or ``message_handler``.
The former stores an optional timeout, while the latter is composable.

Definition and Composition
--------------------------

As the name implies, a ``behavior`` defines the response of an actor to
messages it receives.

.. code-block:: C++

   message_handler x1{
     [](int32_t i) { /*...*/ },
     [](double db) { /*...*/ },
     [](int32_t a, int32_t b, int32_t c) { /*...*/ },
   };

In our first example, ``x1`` models a behavior accepting messages that consist
of either exactly one ``int``, or one ``double``, or three ``int`` values. Any
other message  remains unhandled. When using this behavior in an actor, the
actor will raise an error if it receives a message that does not match any of
the callbacks. To add a default handler, we simply match on ``caf::message``:

.. code-block:: C++

   message_handler x2{
     [](int32_t i) { /*...*/ },
     [](double db) { /*...*/ },
     [](int32_t a, int32_t b, int32_t c) { /*...*/ },
     [](caf::message msg) { /*...*/ },
   };

Using ``caf::message`` as the last callback will consume any message that did
not match any of the previous callbacks. Note that the order is important.
Placing anything after ``caf::message`` will render the latter unreachable. This
is not limited to ``caf::message``, but applies to any callback. For example:

.. code-block:: C++

   message_handler x3{
     [](double db) { /*...*/ },
     [](double db) { /* - unreachable - */ },
   };

This example illustrates an important characteristic of the matching mechanism.
Each message is matched against the callbacks in the order they are defined. The
algorithm stops at the first match. Hence, the second callback in ``x3`` is
unreachable.

.. code-block:: C++

   message_handler x4 = x1.or_else(x3);
   message_handler x5 = x3.or_else(x1);

Message handlers can be combined using ``or_else``. This composition is
not commutative, since the order of callbacks matters. The resulting message
handler will first try to handle a message using the left-hand operand and will
fall back to the right-hand operand if the former did not match. Thus,
``x4`` behaves exactly like ``x1``. This is because the second
callback in ``x1`` will consume any message with a single
``double`` and both callbacks in ``x3`` are thus unreachable.
The handler ``x5`` will consume messages with a single
``double`` using the first callback in ``x3``, essentially
overriding the second callback in ``x1``.

.. _atom:

Atoms
-----

Defining message handlers in terms of callbacks is convenient, but requires a
simple way to annotate messages with meta data. Imagine an actor that provides
a mathematical service for integers. It receives two integers, performs a
user-defined operation and returns the result. Without additional context, the
actor cannot decide whether it should multiply or add the integers. Thus, the
operation must be encoded into the message. The Erlang programming language
introduced an approach to use non-numerical constants, so-called
*atoms*, which have an unambiguous, special-purpose type and do not have
the runtime overhead of string constants.

Atoms in CAF are tag types, i.e., usually defined as en empty ``struct``. These
types carry no data on their own and only exist to annotate messages. For
example, we could create the two tag types ``add_atom`` and ``multiply_atom``
for implementing a simple math actor like this:

.. code-block:: C++

  CAF_BEGIN_TYPE_ID_BLOCK(my_project, caf::first_custom_type_id)

    CAF_ADD_ATOM(my_project, add_atom)
    CAF_ADD_ATOM(my_project, multiply_atom)

  CAF_END_TYPE_ID_BLOCK(my_project)

   behavior do_math{
     [](add_atom, int32_t a, int32_t b) {
       return a + b;
     },
     [](multiply_atom, int32_t a, int32_t b) {
       return a * b;
     }
   };


   // caller side: send(math_actor, add_atom_v, int32_t{1}, int32_t{2})

The macro ``CAF_ADD_ATOM`` defined an empty ``struct`` with the given name as
well as a ``constexpr`` variable for conveniently creating a value of that type
that uses the type name plus a ``_v`` suffix. In the example above,
``atom_value`` is the type name and ``atom_value_v`` is the constant.
