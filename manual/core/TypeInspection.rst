.. _type-inspection:

Type Inspection
===============

We designed CAF with distributed systems in mind. Hence, all message types must
be serializable. Using a message type that is not serializable causes a compiler
error unless explicitly listed as unsafe message type by the user (see
:ref:`unsafe-message-type`). Any unsafe message type may be used only for
messages that remain local, i.e., never cross the wire.

.. _type-inspection-data-model:

Data Model
----------

Type inspection in CAF uses a hierarchical data model with the following
building blocks:

built-in types
  - Signed and unsigned integer types for 8, 16, 32 and 64 bit
  - The floating point types ``float``, ``double`` and ``long double``
  - Bytes, booleans, and strings

lists
  Dynamically-sized container types such as ``std::vector``.

tuples
  Fixed-sized container types such as ``std::tuple`` or ``std::array`` as well
  as built-in C array types.

maps
  Dynamically-sized container types with key/value pairs such as ``std::map``.

objects
  User-defined types. An object has one or more *fields*. Fields have a *name*
  and may be *optional*. Further, fields may take on a fixed number of different
  types.

To see how this maps to C++ types, consider the following type definition:

.. code-block:: C++

  struct test {
    variant<string, double> x1;
    optional<tuple<double, double>> x2;
    vector<string> x3;
  };

Here, field ``x1`` is either a ``string`` or a ``double`` at runtime. The field
``x2`` is optional and may contain a fixed-size tuple with two elements
(built-in types). Lastly, field ``x3`` contains any number of string values at
runtime.

Inspecting Objects
------------------

The inspection API allows CAF to deconstruct C++ objects. Users can either
provide free functions named ``inspect`` that CAF picks up via `ADL
<https://en.wikipedia.org/wiki/Argument-dependent_name_lookup>`_ or specialize
``caf::inspector_access``.

In both cases, users call members and member functions on an ``Inspector`` that
provides a domain-specific language (DSL) for describing the structure of a C++
object.

After listing a custom type ``T`` in a type ID block and either providing a free
``inspect`` function overload or specializing ``inspector_access``, CAF is able
to:

- Serialize and deserialize objects of type ``T`` to/from Byte sequences.
- Render objects of type ``T`` as a human-readable string via
  ``caf::deep_to_string``.
- Read objects of type ``T`` from a configuration file.

In the remainder of this section, we use the following Plain Old Data (POD) type
``point_3d`` in our code examples. Since all member variables of POD types are
*public*, writing custom inspection code is straightforward and we can focus on
the inspection API.

.. code-block:: C++

  struct point_3d {
    int32_t x;
    int32_t y;
    int32_t z;
  };

.. note::

  We strongly recommend using the fixed-width integer types in all user-defined
  messaging types. Consistently using these types over ``short``, ``int``,
  ``long``, etc. avoids bugs in heterogeneous environments that are hard to
  debug.

.. _writing-inspect-overloads:

Writing ``inspect`` Overloads
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Adding overloads for ``inspect`` generally provides the simplest way to teach
CAF how to serialize and deserialize custom data types. We recommend this way of
adding inspection support whenever possible, since it adds the least amount of
boilerplate code.

For our POD type ``point_3d``, we simply pass all member variables as fields to
the inspector:

.. code-block:: C++

  template <class Inspector>
  bool inspect(Inspector& f, point_3d& x) {
    return f.object(x).fields(f.field("x", x.x),
                              f.field("y", x.y),
                              f.field("z", x.z));
  }

As mentioned in the section on the :ref:`data model
<type-inspection-data-model>`, objects are containers for fields that in turn
contain values. When providing an ``inspect`` overload, CAF recursively
traverses all fields.

Not every type needs to expose itself as ``object``, though. For example,
consider the following ID type that simply wraps a string:

.. code-block:: C++

  struct id { std::string value; };

  template <class Inspector>
  bool inspect(Inspector& f, id& x) {
    return f.object(x).fields(f.field("value", x.value));
  }

The type ``id`` is basically a *strong typedef* to improve type safety when
writing code. To a type inspector, ID objects look as follows:

.. code-block:: none

  object(type: "id") {
    field(name: "value") {
      value(type: "string") {
        ...
      }
    }
  }

Now, this type has little use on its own. Usually, we would use such a type to
compose other types such as the following type ``person``:

.. code-block:: C++

  struct person { std::string name; id key; };

  template <class Inspector>
  bool inspect(Inspector& f, person& x) {
    return f.object(x).fields(f.field("name", x.name), f.field("key", x.key));
  }

By providing the ``inspect`` overload for ID, inspectors can recursively visit
an ``id`` as an object. Hence, the above implementations work as expected. When
using ``person`` in human-readable data formats such as CAF configurations,
however, allowing CAF to look "inside" a strong typedef can simplify working
with such types.

With the current implementation, we could read the key ``manager.ceo`` from a
configuration file with this content:

.. code-block:: none

  manager {
    ceo {
      name = "Bob"
      key = {
        value = "TWFuIGlz"
      }
    }
  }

This clearly appears more verbose than it needs to be. Users generally need not
care about such internal types like ``id`` that only exist as a safeguard during
programming.

Hence, we generally recommend making such types transparent to CAF inspectors.
For our ``id`` type, the ``inspect`` overload may instead look as follows:

.. code-block:: C++

  template <class Inspector>
  bool inspect(Inspector& f, id& x) {
    return f.apply(x.value);
  }

In contrast to the previous implementation, inspectors now simply read or write
the strings as values whenever they encounter an ``id``. This simplifies our
config file from before and thus gives a much cleaner interface to users:

.. code-block:: none

  manager {
    ceo {
      name = "Bob"
      key = "TWFuIGlz"
    }
  }

Specializing ``inspector_access``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Working with 3rd party libraries usually rules out adding free functions for
existing classes, because the namespace belongs to a another project. Hence, CAF
also allows specializing ``inspector_access`` instead. This requires writing
more boilerplate code but allows customizing every step of the inspection
process.

The full interface of ``inspector_access`` looks as follows:

.. code-block:: C++

  template <class T>
  struct inspector_access {
    template <class Inspector>
    static bool apply(Inspector& f, T& x);

    template <class Inspector>
    static bool save_field(Inspector& f, string_view field_name, T& x);

    template <class Inspector, class IsPresent, class Get>
    static bool save_field(Inspector& f, string_view field_name,
                           IsPresent& is_present, Get& get);

    template <class Inspector, class IsValid, class SyncValue>
    static bool load_field(Inspector& f, string_view field_name, T& x,
                           IsValid& is_valid, SyncValue& sync_value);

    template <class Inspector, class IsValid, class SyncValue, class SetFallback>
    static bool load_field(Inspector& f, string_view field_name, T& x,
                           IsValid& is_valid, SyncValue& sync_value,
                           SetFallback& set_fallback);
  };

The static member function ``apply`` has the same role as the free ``inspect``
function. For most types, we can implement only ``apply`` and use a default
implementation for the other member functions. For example, specializing
``inspector_access`` for our ``point_3d`` would look as follows:

.. code-block:: C++

  namespace caf {

  template <>
  struct inspector_access<point_3d> : inspector_access_base<point_3d> {
    template <class Inspector>
    static bool apply(Inspector& f, point_3d& x) {
      return f.object(x).fields(f.field("x", x.x),
                                f.field("y", x.y),
                                f.field("z", x.z));
    }
  };

  } // namespace caf

By inheriting from ``inspector_access_base``, we use the default implementations
for ``save_field`` and ``load_field``. Customizing this set of functions only
becomes necessary when integration custom types that have semantics similar to
``tuple``, ``variant``, or ``optional``.

.. note::

  Please refer to the Doxygen documentation for more details on ``save_field``
  and ``load_field``.

Types with Getter and Setter Access
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

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
whether the operation succeeded (by returning ``true``) or failed (by returning
``false``).

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

Fallbacks and Invariants
~~~~~~~~~~~~~~~~~~~~~~~~

For each field, we may provide a fallback value for optional fields or a
predicate that checks invariants on the data (or both). For example, consider
the following class ``duration`` and its implementation for ``inspect``:

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

In "real code", we probably would not use a ``string`` to store the time unit.
However, with the fallback, we have enabled CAF to use ``"seconds"`` whenever
the input contains no value for the ``unit`` field. Further, the invariant makes
sure that we verify our input before accepting it.

With this implementation for ``inspect``, we could use ``duration`` in a
configuration files as follows (assuming a parameter named
``example-app.request-timeout``):

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

Splitting Save and Load
-----------------------

When writing custom ``inspect`` functions, providing a single overload for all
inspectors may result in undesired tradeoffs or convoluted code. Sometimes,
inspection code can benefit from splitting it into a ``save`` and a ``load``
function. For this reason, all inspector provide a static constant called
``is_loading``. This allows delegating to custom functions via ``enable_if`` or
``if constexpr``:

.. code-block:: C++

  template <class Inspector>
  bool inspect(Inspector& f, my_class& x) {
    if constexpr (Inspector:is_loading)
      return load(f, x);
    else
      return save(f, x);
  }

.. _has-human-readable-format:

Specializing on the Data Format
-------------------------------

Much like ``is_loading`` allows client code to dispatch based on the mode of an
inspector, the member function ``has_human_readable_format()`` allows client
code to dispatch based on the data format.

The canonical example for choosing a different data representation for
human-readable input and output is the ``enum`` type. When generating data for
machine-to-machine communication, using the underlying integer representation
gives the best performance. However, using the constant names results in a much
better user experience in all other cases.

The following code illustrates how to provide a string representation for
inspectors that operate on human-readable data representations while operating
directly on the underlying type of the ``enum class`` otherwise.

.. code-block:: C++

  enum class weekday : uint8_t {
    monday,
    tuesday,
    wednesday,
    thursday,
    friday,
    saturday,
    sunday,
  };

  std::string to_string(weekday);

  bool parse(std::string_view input, weekday& dest);

  template <class Inspector>
  bool inspect(Inspector& f, weekday& x) {
    if (f.has_human_readable_format()) {
      auto get = [&x] { return to_string(x); };
      auto set = [&x](std::string str) { return parse(str, x); };
      return f.apply(get, set);
    } else {
      auto get = [&x] { return static_cast<uint8_t>(x); };
      auto set = [&x](uint8_t val) {
        if (val < 7) {
          x = static_cast<weekday>(val);
          return true;
        } else {
          return false;
        }
      };
      return f.apply(get, set);
    }
  }

When inspecting an object of type ``weekday``, we treat is as if it were a
string for inspectors with human-readable data formats. Otherwise, we treat the
weekday as if it were an integer between 0 and 6.

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
