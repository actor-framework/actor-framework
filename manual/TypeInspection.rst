.. _type-inspection:

Type Inspection
===============

We designed CAF with distributed systems in mind. Hence, all message types must
be serializable. Using a message type that is not serializable causes a compiler
error unless explicitly listed as unsafe message type by the user (see
:ref:`unsafe-message-type`).

The inspection API allows CAF to deconstruct C++ objects into fields and values.
Users can either provide free functions named ``inspect`` that CAF picks up via
`ADL <https://en.wikipedia.org/wiki/Argument-dependent_name_lookup>`_ or
specialize ``caf::inspector_access``.

In both cases, users call members and member functions on an ``Inspector`` that
provides a domain-specific language (DSL) for describing the structure of a C++
object.

POD Types
---------

Plain old data (POD) types always declare all member variables *public*. An
``inspect`` overload for PODs simply passes all member variables as fields to
the *inspector*. For example, consider the following POD type ``point_3d``:

.. code-block:: C++

  struct point_3d {
    int x;
    int y;
    int z;
  };

To allow CAF to properly serialize and deserialize the POD type, the simplest
way is to write a free ``inspect`` function:

.. code-block:: C++

  template <class Inspector>
  bool inspect(Inspector& f, point_3d& x) {
    return f.object().fields(f.field("x", x.x),
                             f.field("y", x.y),
                             f.field("z", x.z));
  }

After providing this function as well as listing ``point_3d`` in a type ID
block, CAF can save and load our custom POD type. Of course, this is a recursive
process that allows us to use ``point_3d`` as member variables of other types.

Working with 3rd party libraries usually rules out adding free functions for
existing classes, because the namespace belongs to another project. Hence, CAF
also allows specializing ``inspector_access`` instead. This requires slightly
more boilerplate code, but follows the same pattern:

.. code-block:: C++

  namespace caf {
    template <>
    struct inspector_access<point_3d> {
      template <class Inspector>
      static bool apply(Inspector& f, point_3d& x) {
        return f.object(x).fields(f.field("x", x.x),
                                  f.field("y", x.y),
                                  f.field("z", x.z));
      }
    };
  }

After listing ``point_3d`` in a type ID block and either providing a free
``inspect`` overload or specializing ``inspector_access``, CAF is able to:

- Serialize and deserialize ``point_3d`` objects to/from Byte sequences.
- Render a ``point_3d`` as a human-readable string via ``caf::deep_to_string``.
- Read ``point_3d`` objects from a configuration file.

Types with Getter and Setter Access
-----------------------------------

Types that declare their fields *private* and only grant access via getter and
setter cannot pass references to the member variables to the inspector. Instead,
they can pass a pair of function objects to the inspector to read and write the
field.

Consider the following non-POD type ``foobar``:

.. code-block:: C++

  class foobar {
  public:
    const std::string& foo() {
      return foo_;
    }

    void foo(std::string value) {
      foo_ = std::move(value);
    }

    const std::string& bar() {
      return bar_;
    }

    void bar(std::string value) {
      bar_ = std::move(value);
    }

  private:
    std::string foo_;
    std::string bar_;
  };

Since ``foo_`` and ``bar_`` are not accessible from outside the class, the
inspector has to use the getter and setter functions. However, C++ has no
formalized API for getters and setters. Moreover, not all setters are so trivial
as in the example above. Setters may enforce invariants, for example, and thus
may fail.

In order to work with any flair of getter and setter functions, CAF requires
users to wrap these member functions calls into two function objects. The first
one wraps the getter, takes no arguments, and returns the underlying value
(either by reference or by value). The second one wraps the setter, takes
exactly one argument (the new value), and returns a ``bool`` that indicates
whether the operation succeeded (by returning ``true``) or failed (``by
returning false``).

The example below shows a possible ``inspect`` implementation for the ``fobar``
class shown before:

.. code-block:: C++

  template <class Inspector>
  bool inspect(Inspector& f, foobar& x) {
    auto get_foo = [&x]() -> decltype(auto) { return x.foo(); };
    auto set_foo = [&x](std::string value) {
      x.foo(std::move(value));
      return true;
    };
    auto get_bar = [&x]() -> decltype(auto) { return x.bar(); };
    auto set_bar = [&x](std::string value) {
      x.bar(std::move(value));
      return true;
    };
    return f.object(x).fields(f.field("foo", get_foo, set_foo),
                              f.field("bar", get_bar, set_bar));
  }

.. note::

  For classes that lie in the responsibility of the same developers that
  implement the ``inspect`` function, implementing ``inspect`` as friend
  function inside the class usually can avoid going through the getter and
  setter functions.

Inspector DSL
-------------

As shown in previous examples, type inspection with an inspector ``f`` always
starts with ``f.object(...)`` as entry point. After that, users may set a pretty
type name with ``pretty_name`` that inspectors may use when generating
human-readable output. Afterwards, the inspectors expect users to call
``fields`` with all member variables of the object.

The following pseudo code illustrates how the inspector DSL is structured in
terms of types involved and member function interfaces.


.. code-block:: C++

  class Inspector {
  public:
    Object object(T obj);

    Field field(string_view name, T& ref);

    Field field(string_view name, function<T()> get, function<void(T)> set);
  };

  class FieldWithFallbackAndInvariant;

  class FieldWithFallback {
  public:
    FieldWithFallbackAndInvariant invariant(function<bool>(T));
  };

  class FieldWithInvariant;

  class Field {
  public:
    FieldWithFallback fallback(T);

    FieldWithInvariant invariant(function<bool>(T));
  };

  class FieldsInspector {
  public:
    bool fields(...);
  };

  class Object : public FieldsInspector {
  public:
    FieldsInspector pretty_name(string_view);
  };

The ``Inspector`` has the ``object`` member variable to set things in motion,
but it also serves as a factory for ``Field`` definitions. Fields always have a
name and users can either bind a reference to the member variable or provide
getter and setter functions. Optionally, users can equip fields with a fallback
value or an invariant predicate. Providing a fallback value automatically makes
fields optional. For example, consider the following class ``duration`` and its
implementation for ``inspect``:

.. code-block:: C++

  struct duration {
    string unit;
    double count;
  };

  bool valid_time_unit(const string& unit) {
    return unit == "seconds" || unit == "minutes";
  }

  template <class Inspector>
  bool inspect(Inspector& f, duration& x) {
    return f.object(x).fields(
      f.field("unit", x.unit).fallback("seconds").invariant(valid_time_unit),
      f.field("count", x.count));
  }

Real code probably would not use a ``string`` to store the time unit. However,
with the fallback, we have enabled CAF to use ``"seconds"`` whenever the input
contains no value for the ``unit`` field. Further, the invariant makes sure that
we verify our input before accepting it.

Whether optional fields are supported depends on the format / inspector. For
example, the binary serialization protocol in CAF has no notion of optional
fields. Hence, the default binary serializers simply read and write fields in
the order they apear in. However, the inspectors for the configuration framework
do allow optional fields. With our ``inspect`` overload for ``duration``, we
could configure a parameter named ``example-app.request-timeout`` as follows:

.. code-block:: none

  # example 1: ok, falls back to "seconds"
  example-app {
    request-timeout {
      count = 1.3
    }
  }

  # example 2: ok, explicit definition of the time unit
  example-app {
    request-timeout {
      count = 1.3
      unit = "minutes"
    }
  }

  # example 3: error, "parsecs" is not a time unit (invariant does not hold)
  example-app {
    request-timeout {
      count = 12
      unit = "parsecs"
    }
  }

Inspector Traits
----------------

When writing custom ``inspect`` functions, providing a single overload for all
inspectors may result in undesired tradeoffs or convoluted code. For example,
inspection code may be much cleaner when split into a ``save`` and a ``load``
function. The kind of output format may also play a role. When reading and
writing human-readable data, we might want to improve the user experience by
using the constant names in enumeration types rather than using the underlying
integer values.

To enable static dispatching based on the inspector kind, all inspectors provide
the following ``static constexpr bool`` constants:

- ``is_loading``: If ``true``, tags the inspector as a deserializer that
  overrides the state of an object. Otherwise, the inspector is *saving*, i.e.,
  it visits the state of an object without modifying it.
- ``has_human_readable_format``: If ``true``, tags the inspector as reading and
  writing a data format that may be consumed or generated by humans. Otherwise,
  the ``inspect`` overload can assume a binary data format and optimize for
  space rather than user experience. For example, an ``inspect`` overload for
  enumeration types may use the constant names for a human-readable format but
  otherwise use the underlying integer values.

Using theses constants, users can easily split ``inspect`` overloads into *load*
and *safe*:

.. code-block:: C++

  template <class Inspector>
  bool inspect(Inspector& f, my_class& x) {
    if constexpr (Inspector:is_loading)
      return load(f, x);
    else
      return save(f, x);
  }

.. _unsafe-message-type:

Unsafe Message Types
--------------------

Message types that do not provide serialization code cause compile time errors
when used in actor communication. When using CAF for concurrency only, this
errors can be suppressed by explicitly allowing types via
``CAF_ALLOW_UNSAFE_MESSAGE_TYPE``. The macro is defined as follows.

.. code-block:: C++

  #define CAF_ALLOW_UNSAFE_MESSAGE_TYPE(type_name)                             \
    namespace caf {                                                            \
    template <>                                                                \
    struct allowed_unsafe_message_type<type_name> : std::true_type {};         \
    }

Keep in mind that *unsafe* means that your program runs into undefined behavior
(or segfaults) when you break your promise and try to serialize messages that
contain unsafe message types.

.. note::

  Even *unsafe* messages types still require a :ref:`type ID
  <custom-message-types>`.

.. _custom-inspectors:

Custom Inspectors (Serializers and Deserializers)
-------------------------------------------------

Writing custom serializers and deserializers enables users to add support for
alternative wire formats such as `Google Protocol Buffers
<https://developers.google.com/protocol-buffers>`_ or `MessagePack
<https://msgpack.org/index.html>`_ as well as supporting non-binary formats such
as `XML <https://www.w3.org/XML>`_ or `JSON
<https://www.json.org/json-en.html>`_.
