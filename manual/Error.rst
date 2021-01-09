.. _error:

Errors
======

Errors in CAF have a code and a category, similar to ``std::error_code`` and
``std::error_condition``. Unlike its counterparts from the C++ standard library,
``error`` is plattform-neutral and serializable.

Class Interface
---------------

+-----------------------------------------+--------------------------------------------------------------------+
| **Constructors**                        |                                                                    |
+-----------------------------------------+--------------------------------------------------------------------+
| ``(Enum code)``                         | Constructs an error with given error code.                         |
+-----------------------------------------+--------------------------------------------------------------------+
| ``(Enum code, messag context)``         | Constructs an error with given error code and additional context.  |
+-----------------------------------------+--------------------------------------------------------------------+
|                                         |                                                                    |
+-----------------------------------------+--------------------------------------------------------------------+
| **Observers**                           |                                                                    |
+-----------------------------------------+--------------------------------------------------------------------+
| ``uint8_t code()``                      | Returns the error code as 8-bit integer.                           |
+-----------------------------------------+--------------------------------------------------------------------+
| ``type_id_t category()``                | Returns the type ID of the Enum type used to construct this error. |
+-----------------------------------------+--------------------------------------------------------------------+
| ``message context()``                   | Returns additional context information                             |
+-----------------------------------------+--------------------------------------------------------------------+
| ``explicit operator bool()``            | Returns ``code() != 0``                                            |
+-----------------------------------------+--------------------------------------------------------------------+

.. _custom-error:

Add Custom Error Categories
---------------------------

Adding custom error categories requires these steps:

* Declare an enum class of type ``uint8_t`` with error codes starting at 1. CAF
  always interprets the value 0 as *no error*.

* Assign a type ID to your enum type.

* Specialize ``caf::is_error_code_enum`` for your enum type. For this step, CAF
  offers the macro ``CAF_ERROR_CODE_ENUM`` to generate the boilerplate code
  necessary.

The following example illustrates all these steps for a custom error code enum
called ``math_error``.

.. literalinclude:: /examples/message_passing/divider.cpp
   :language: C++
   :lines: 17-47

.. _sec:

Default Error Codes
-------------------

The enum type ``sec`` (for System Error Code) provides many error codes for
common failures in actor systems:

.. literalinclude:: /libcaf_core/caf/sec.hpp
   :language: C++
   :start-after: --(rst-sec-begin)--
   :end-before: --(rst-sec-end)--

.. _exit-reason:

Default Exit Reasons
--------------------

A special kind of error codes are exit reasons of actors. These errors are
usually fail states set by the actor system itself. The two exceptions are
``exit_reason::user_shutdown`` and ``exit_reason::kill``. The former is used in
CAF to signalize orderly, user-requested shutdown and can be used by programmers
in the same way. The latter terminates an actor unconditionally when used in
``send_exit``, even for actors that override the default handler (see
:ref:`exit-message`).

.. literalinclude:: /libcaf_core/caf/exit_reason.hpp
   :language: C++
   :start-after: --(rst-exit-reason-begin)--
   :end-before: --(rst-exit-reason-end)--
