.. _error:

Errors
======

Errors in CAF have a code and a category, similar to ``std::error_code`` and
``std::error_condition``. Unlike its counterparts from the C++ standard library,
``error`` is plattform-neutral and serializable. Instead of using category
singletons, CAF stores categories as atoms (see :ref:`atom`). Errors can also
include a message to provide additional context information.

Class Interface
---------------

+-----------------------------------------+--------------------------------------------------------------------+
| **Constructors**                        |                                                                    |
+-----------------------------------------+--------------------------------------------------------------------+
| ``(Enum x)``                            | Construct error by calling ``make_error(x)``                       |
+-----------------------------------------+--------------------------------------------------------------------+
| ``(uint8_t x, atom_value y)``           | Construct error with code ``x`` and category ``y``                 |
+-----------------------------------------+--------------------------------------------------------------------+
| ``(uint8_t x, atom_value y, message z)``| Construct error with code ``x``, category ``y``, and context ``z`` |
+-----------------------------------------+--------------------------------------------------------------------+
|                                         |                                                                    |
+-----------------------------------------+--------------------------------------------------------------------+
| **Observers**                           |                                                                    |
+-----------------------------------------+--------------------------------------------------------------------+
| ``uint8_t code()``                      | Returns the error code                                             |
+-----------------------------------------+--------------------------------------------------------------------+
| ``atom_value category()``               | Returns the error category                                         |
+-----------------------------------------+--------------------------------------------------------------------+
| ``message context()``                   | Returns additional context information                             |
+-----------------------------------------+--------------------------------------------------------------------+
| ``explicit operator bool()``            | Returns ``code() != 0``                                            |
+-----------------------------------------+--------------------------------------------------------------------+

.. _custom-error:

Add Custom Error Categories
---------------------------

Adding custom error categories requires three steps: (1) declare an enum class
of type ``uint8_t`` with the first value starting at 1, (2) specialize
``error_category`` to give your type a custom ID (value 0-99 are
reserved by CAF), and (3) add your error category to the actor system config.
The following example adds custom error codes for math errors.

.. literalinclude:: /examples/message_passing/divider.cpp
   :language: C++
   :lines: 17-47

.. _sec:

System Error Codes
------------------

System Error Codes (SECs) use the error category ``"system"``. They
represent errors in the actor system or one of its modules and are defined as
follows.

.. literalinclude:: /libcaf_core/caf/sec.hpp
   :language: C++
   :lines: 32-117

.. _exit-reason:

Default Exit Reasons
--------------------

CAF uses the error category ``"exit"`` for default exit reasons. These errors
are usually fail states set by the actor system itself. The two exceptions are
``exit_reason::user_shutdown`` and ``exit_reason::kill``. The former is used in
CAF to signalize orderly, user-requested shutdown and can be used by programmers
in the same way. The latter terminates an actor unconditionally when used in
``send_exit``, even if the default handler for exit messages (see
:ref:`exit-message`) is overridden.

.. literalinclude:: /libcaf_core/caf/exit_reason.hpp
   :language: C++
   :lines: 29-49

