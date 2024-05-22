.. _hashing:

Hashing
=======

Writing an ``inspect`` overload (see :ref:`type-inspection`), for a custom type
not only enables CAF to serialize and deserialize it, but also to generate hash
values for it.

Consider this simple POD type with an ``inspect`` overload:

.. code-block:: C++

  struct point_3d {
    int32_t x;
    int32_t y;
    int32_t z;
  };

  template<class Inspector>
  bool inspect(Inspector& f, point_3d& point) {
    return f.object(point).fields(f.field("x", point.x),
                                  f.field("y", point.y),
                                  f.field("z", point.z));
  }

The common algorithm of choice for generating hash values is the FNV_ algorithm,
which is designed for hash tables and fast. Because this algorithm is so common,
CAF ships an inspector that implements it: ``caf::hash::fnv`` (include
``caf/hash/fnv.hpp``).

The FNV algorithm chooses a different prime number as the seed value based on
whether you are generating a 32-bit hash value or a 64-bit hash value. In CAF,
you choose the seed implicitly by instantiating the inspector with ``uint32_t``,
``uint64_t``, or ``size_t``.

Because applying any value to the inspector always results in an integer value,
``caf::hash::fnv`` has a static member function called ``compute`` that takes any
number of inspectable values. This means generating a hash boils down to a
one-liner!

It also makes it very convenient to specialize ``std::hash``. For our
``point_3d``, the implementation boils down to this:

.. code-block:: C++

  namespace std {

  template <>
  struct hash<point_3d> {
    size_t operator()(const point_3d& point) const noexcept {
      return caf::hash::fnv<size_t>::compute(point);
    }
  };

  } // namespace std

Under the hood, the inspector uses the ``inspect`` overload to traverse the
object. Hash inspectors ignore type names, field names, etc. So passing a
``point_3d`` to the inspector results in the same result as passing ``x``, ``y``
and ``z`` individually:

.. code-block:: C++

  using hasher = caf::hash::fnv<uint32_t>;
  sys.println("hash of (1, 2, 3): {}", hasher::compute(1, 2, 3));
  sys.println("hash of point_3d(1, 2, 3): {}", hasher::compute(point_3d{1, 2, 3}));

In the code above, the result of both calls to ``hasher::compute`` will be the
FNV hash value of the sequence ``1, 2, 3``, i.e., ``2034659765``.

.. _FNV: https://en.wikipedia.org/wiki/FNV_hash_function
