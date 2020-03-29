.. _type-inspection:

Type Inspection (Serialization and String Conversion)
=====================================================

CAF is designed with distributed systems in mind. Hence, all message types must
be serializable and need a platform-neutral, unique name that is configured at
startup (see :ref:`add-custom-message-type`). Using a message type that is not
serializable causes a compiler error (see :ref:`unsafe-message-type`). CAF
serializes individual elements of a message by using the inspection API. This
API allows users to provide code for serialization as well as string conversion
with a single free function. The signature for a class ``my_class`` is always as
follows:

.. code-block:: C++

   template <class Inspector>
   typename Inspector::result_type inspect(Inspector& f, my_class& x) {
     return f(...);
   }

The function ``inspect`` passes meta information and data fields to the
variadic call operator of the inspector. The following example illustrates an
implementation for ``inspect`` for a simple POD struct.

.. literalinclude:: /examples/custom_type/custom_types_1.cpp
   :language: C++
   :lines: 23-33

The inspector recursively inspects all data fields and has builtin support for
(1) ``std::tuple``, (2) ``std::pair``, (3) C arrays, (4) any
container type with ``x.size()``, ``x.empty()``,
``x.begin()`` and ``x.end()``.

We consciously made the inspect API as generic as possible to allow for
extensibility. This allows users to use CAF's types in other contexts, to
implement parsers, etc.

Inspector Concept
-----------------

The following concept class shows the requirements for inspectors. The
placeholder ``T`` represents any user-defined type. For example,
``error`` when performing I/O operations or some integer type when
implementing a hash function.

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

Annotations
-----------

Annotations allow users to fine-tune the behavior of inspectors by providing
addition meta information about a type. All annotations live in the namespace
``caf::meta`` and derive from ``caf::meta::annotation``. An
inspector can query whether a type ``T`` is an annotation with
``caf::meta::is_annotation<T>::value``. Annotations are passed to the
call operator of the inspector along with data fields. The following list shows
all annotations supported by CAF:

* ``type_name(n)``: Display type name as ``n`` in  human-friendly output (position before data fields).
* ``hex_formatted()``: Format the following data field in hex  format.
* ``omittable()``: Omit the following data field in human-friendly  output.
* ``omittable_if_empty()``: Omit the following data field if it is  empty in human-friendly output.
* ``omittable_if_none()``: Omit the following data field if it  equals ``none`` in human-friendly output.
* ``save_callback(f)``: Call ``f`` when serializing  (position after data fields).
* ``load_callback(f)``: Call ``f`` after deserializing all  data fields (position after data fields).

Backwards and Third-party Compatibility
---------------------------------------

CAF evaluates common free function other than ``inspect`` in order to
simplify users to integrate CAF into existing code bases.

Serializers and deserializers call user-defined ``serialize``
functions. Both types support ``operator&`` as well as
``operator()`` for individual data fields. A ``serialize``
function has priority over ``inspect``.

When converting a user-defined type to a string, CAF calls user-defined
``to_string`` functions and prefers those over ``inspect``.

.. _unsafe-message-type:

Whitelisting Unsafe Message Types
---------------------------------

Message types that are not serializable cause compile time errors when used in
actor communication. When using CAF for concurrency only, this errors can be
suppressed by whitelisting types with
``CAF_ALLOW_UNSAFE_MESSAGE_TYPE``. The macro is defined as follows.

Splitting Save and Load Operations
----------------------------------

If loading and storing cannot be implemented in a single function, users can
query whether the inspector is loading or storing. For example, consider the
following class ``foo`` with getter and setter functions and no public
access to its members.

.. literalinclude:: /examples/custom_type/custom_types_3.cpp
   :language: C++
   :start-after: --(rst-foo-begin)--
   :end-before: --(rst-foo-end)--

Since there is no access to the data fields ``a_`` and ``b_``
(and assuming no changes to ``foo`` are possible), we need to split our
implementation of ``inspect`` as shown below.

.. literalinclude:: /examples/custom_type/custom_types_3.cpp
   :language: C++
   :start-after: --(rst-inspect-begin)--
   :end-before: --(rst-inspect-end)--

The purpose of the scope guard in the example above is to write the content of
the temporaries back to ``foo`` at scope exit automatically. Storing
the result of ``f(...)`` in a temporary first and then writing the
changes to ``foo`` is not possible, because ``f(...)`` can
return ``void``.
