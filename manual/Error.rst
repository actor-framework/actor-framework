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

+------------------------------------------+--------------------------------------------------------------------+
| **Constructors**                                                                                              |
+------------------------------------------+--------------------------------------------------------------------+
| ``(Enum x)``                             | Construct error by calling ``make_error(x)``                       |
+------------------------------------------+--------------------------------------------------------------------+
| ``(uint8_t x, atom_value y)``            | Construct error with code ``x`` and category ``y``                 |
+------------------------------------------+--------------------------------------------------------------------+
| ``(uint8_t x, atom_value y, message z)`` | Construct error with code ``x``, category ``y``, and context ``z`` |
+------------------------------------------+--------------------------------------------------------------------+
| **Observers**                                                                                                 |
+------------------------------------------+--------------------------------------------------------------------+
| ``uint8_t code()``                       | Returns the error code                                             |
+------------------------------------------+--------------------------------------------------------------------+
| ``atom_value category()``                | Returns the error category                                         |
+------------------------------------------+--------------------------------------------------------------------+
| ``message context()``                    | Returns additional context information                             |
+------------------------------------------+--------------------------------------------------------------------+
| ``explicit operator bool()``             | Returns ``code() != 0``                                            |
+------------------------------------------+--------------------------------------------------------------------+

.. _custom-error:

Add Custom Error Categories
---------------------------

Adding custom error categories requires two steps: (1) declare an enum class of
type ``uint8_t`` with the first value starting at 1, (2) enable conversions from
the error code enum to ``error`` by using the macro ``CAF_ERROR_CODE_ENUM``. The
following example illustrates these two steps with a ``math_error`` enum to
signal divisions by zero:

.. literalinclude:: /examples/message_passing/divider.cpp
   :language: C++
   :start-after: --(rst-divider-begin)--
   :end-before: --(rst-divider-end)--

.. note::

   CAF stores the integer value with an atom that disambiguates error codes from
   different categories. The macro ``CAF_ERROR_CODE_ENUM`` uses the given type
   name as atom as well. This means the same limitations apply. In particular,
   10 characters or less. If the type name exceeds this limit, users can pass
   two arguments instead. In the example above, we could call
   ``CAF_ERROR_CODE_ENUM(math_error, "math")`` instead.

.. _sec:

System Error Codes
------------------

System Error Codes (SECs) represent errors in the actor system or one of its
modules and are defined as follows:

.. literalinclude:: /libcaf_core/caf/sec.hpp
   :language: C++
   :start-after: --(rst-sec-begin)--
   :end-before: --(rst-sec-end)--

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
   :start-after: --(rst-exit-reason-begin)--
   :end-before: --(rst-exit-reason-end)--
