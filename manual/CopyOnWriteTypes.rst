.. _copy-on-write-types:

Copy-on-Write Types
===================

Copy-on-write (COW) is an optimization technique that makes copies cheap by
implicitly sharing data between the original and the copy until one of them is
modified. This makes it convenient and cheap to pass values around while also
making sure that the data is only copied when necessary.

CAF uses copy-on-write for its ``message`` type, which basically is a
type-erased tuple.  CAF also includes ``cow_tuple`` which wraps a ``std::tuple``
and allows users to pass e expensive data (like strings and lists) as a single
unit that can be passed around cheaply. This is particularly useful when
emitting tuples in a flow, where the tuples are frequently buffered and passed
around.

Here is a small example to illustrate the API:

.. code-block:: C++

  auto addr = [](auto& tup) {
    return reinterpret_cast<intptr_t>(std::addressof(tup.data()));
  };
  auto xs = caf::make_cow_tuple(1, 2, 3);
  auto ys = xs;
  sys.println("After initializing:");
  sys.println("xs: {} ({:#x})", xs, addr(xs));
  sys.println("ys: {} ({:#x})", ys, addr(ys));
  ys.unshared() = std::tuple{4, 5, 6};
  sys.println("After assigning to ys:");
  sys.println("xs: {} ({:#x})", xs, addr(xs));
  sys.println("ys: {} ({:#x})", ys, addr(ys));

An example output of this program is:

.. code-block:: text

  After initializing:
  xs: [1, 2, 3] (0x10a001d30)
  ys: [1, 2, 3] (0x10a001d30)
  After assigning to ys:
  xs: [1, 2, 3] (0x10a001d30)
  ys: [4, 5, 6] (0x10a001d00)

By default, ``cow_tuple`` only grants ``const`` access to its elements. With
``get<I>(xs)``, users can get access to a single value of ``xs`` at the index
``I``. With ``xs.data()``, users get a ``const`` reference to the internally
stored ``std::tuple``.

In order to gain mutable access, users may call ``unshared()`` to get a mutable
reference to the ``std::tuple``. This function makes a deep copy of the data if
there is more than one reference to the data at the moment. In our example
above, ``ys`` initially points to the same data as ``xs``. After calling
``unshared()`` on ``ys``, however, the two tuples point to different data.

Especially when using the flow API, it is important to use values that are
cheap to copy. For this reason, CAF includes a few convenience types that wrap
their standard equivalent:

- ``cow_string``: A copy-on-write string that wraps ``std::string``.
- ``cow_vector``: A copy-on-write vector that wraps ``std::vector``.
- ``cow_tuple``: A copy-on-write tuple that wraps ``std::tuple``.
