.. _type-inspection:

Type Inspection
===============

CAF is designed with distributed systems in mind. Hence, all message types must
be serializable and need a platform-neutral, unique name (see
:ref:`add-custom-message-type`). Using a message type that is not serializable
causes a compiler error (see :ref:`unsafe-message-type`). CAF serializes
individual elements of a message by using the inspection API. This API allows
users to provide code for serialization as well as string conversion, usually
with a single free function. The signature for a class ``my_class`` is always as
follows:

.. code-block:: C++

   template <class Inspector>
   typename Inspector::result_type inspect(Inspector& f, my_class& x) {
     return f(...);
   }

The function ``inspect`` passes meta information and data fields to the
variadic call operator of the inspector. The following example illustrates an
implementation of ``inspect`` for a simple POD called ``foo``.

.. literalinclude:: /examples/custom_type/custom_types_1.cpp
   :language: C++
   :start-after: --(rst-foo-begin)--
   :end-before: --(rst-foo-end)--

The inspector recursively inspects all data fields and has builtin support for
``std::tuple``, ``std::pair``, arrays, as well as containers that provide the
member functions ``size``, ``empty``, ``begin`` and ``end``.

We consciously made the inspect API as generic as possible to allow for
extensibility. This allows users to use CAF's types in other contexts, to
implement parsers, etc.

.. note::

  When converting a user-defined type to a string, CAF calls user-defined
  ``to_string`` functions and prefers those over ``inspect``.

Inspector Concept
-----------------

The following concept class shows the requirements for inspectors. The
placeholder ``T`` represents any user-defined type. Usually ``error`` or
``error_code``.

.. code-block:: C++

   Inspector {
     using result_type = T;

     static constexpr bool reads_state = ...;

     static constexpr bool writes_state = ...;

     template <class... Ts>
     result_type operator()(Ts&&...);
   }

A saving ``Inspector`` is required to handle constant lvalue and rvalue
references. A loading ``Inspector`` must only accept mutable lvalue
references to data fields, but still allow for constant lvalue references and
rvalue references to annotations.

Inspectors that only visit data fields (such as a serializer) sets
``reads_state`` to ``true`` and ``writes_state`` to ``false``. Inspectors that
override data fields (such as a deserializer) assign the inverse values.

These compile-time constants can be used in ``if constexpr`` statements or to
select different ``inspect`` overloads with ``enable_if``.

Annotations
-----------

Annotations allow users to fine-tune the behavior of inspectors by providing
addition meta information about a type. All annotations live in the namespace
``caf::meta`` and derive from ``caf::meta::annotation``. An
inspector can query whether a type ``T`` is an annotation with
``caf::meta::is_annotation<T>::value``. Annotations are passed to the
call operator of the inspector along with data fields. The following list shows
all annotations supported by CAF:

- ``type_name(n)``: Display type name as ``n`` in  human-friendly output
  (position before data fields).
- ``hex_formatted()``: Format the following data field in hex  format.
- ``omittable()``: Omit the following data field in human-friendly output.
- ``omittable_if_empty()``: Omit the following data field in human-friendly
  output if it is empty.
- ``omittable_if_none()``: Omit the following data field in human-friendly
  output if it equals ``none``.
- ``save_callback(f)``: Call ``f`` if ``reads_state == true``. Pass this
  callback after the data fields.
- ``load_callback(f)``: Call ``f`` ``writes_state == true``. Pass this callback
  after the data fields.

Backwards and Third-party Compatibility
---------------------------------------

CAF evaluates common free function other than ``inspect`` in order to simplify
users to integrate CAF into existing code bases.

Serializers and deserializers call user-defined ``serialize`` functions. Both
types support ``operator&`` as well as ``operator()`` for individual data
fields. A ``serialize`` function has priority over ``inspect``.

.. _unsafe-message-type:

Whitelisting Unsafe Message Types
---------------------------------

Message types that are not serializable cause compile time errors when used in
actor communication. For messages that never cross the network, this errors can
be suppressed by whitelisting types with ``CAF_ALLOW_UNSAFE_MESSAGE_TYPE``. The
macro is defined as follows.

.. literalinclude:: /libcaf_core/caf/allowed_unsafe_message_type.hpp
   :language: C++
   :start-after: --(rst-macro-begin)--
   :end-before: --(rst-macro-end)--

Keep in mind that ``unsafe`` means that your program runs into undefined
behavior (or segfaults) when you break your promise and try to serialize
messages that contain unsafe message types.

Saving and Loading with Getters and Setters
-------------------------------------------

Many classes shield their member variables with getter and setter functions.
This can be addressed by declaring the ``inspect`` function as ``friend``, but
only when not dealing with 3rd party library types. For example, consider the
following class ``foo`` with getter and setter functions and no public access to
its members.

.. literalinclude:: /examples/custom_type/custom_types_3.cpp
   :language: C++
   :start-after: --(rst-foo-begin)--
   :end-before: --(rst-foo-end)--

Since there is no access to the data fields ``a_`` and ``b_`` (and assuming no
changes to ``foo`` are possible), we can serialize or deserialize from/to local
variables and use a load callback to write back to the object when loading
state.

.. literalinclude:: /examples/custom_type/custom_types_3.cpp
   :language: C++
   :start-after: --(rst-inspect-begin)--
   :end-before: --(rst-inspect-end)--

For more complicated cases, we can also split the inspect overload as follows:

.. code-block:: C++

   template <class Inspector>
   typename std::enable_if<Inspector::reads_state,
                           typename Inspector::result_type>::type
   inspect(Inspector& f, my_class& x) {
     // ... serializing ...
   }

   template <class Inspector>
   typename std::enable_if<Inspector::writes_state,
                           typename Inspector::result_type>::type
   inspect(Inspector& f, my_class& x) {
     // ... deserializing ...
   }
