.. _reference-counting:

Reference Counting
==================

Actors systems can span complex communication graphs that make it hard to
decide when actors are no longer needed. As a result, manually managing
lifetime of actors is merely impossible. For this reason, CAF implements a
garbage collection strategy for actors based on weak and strong reference
counts.

Shared Ownership in C++
-----------------------

The C++ standard library already offers ``shared_ptr`` and
``weak_ptr`` to manage objects with complex shared ownership. The
standard implementation is a solid general purpose design that covers most use
cases. Weak and strong references to an object are stored in a *control
block*. However, CAF uses a slightly different design. The reason for this is
twofold. First, we need the control block to store the identity of an actor.
Second, we wanted a design that requires less indirections, because actor
handles are used extensively copied for messaging, and this overhead adds up.

Before discussing the approach to shared ownership in CAF, we look at the
design of shared pointers in the C++ standard library.

.. _shared-ptr:

.. image:: shared_ptr.png
   :alt: Shared pointer design in the C++ standard library

The figure above depicts the default memory layout when using shared pointers.
The control block is allocated separately from the data and thus stores a
pointer to the data. This is when using manually-allocated objects, for example
``shared_ptr<int> iptr{new int}``. The benefit of this design is that
one can destroy ``T`` independently from its control block. While
irrelevant for small objects, it can become an issue for large objects.
Notably, the shared pointer stores two pointers internally. Otherwise,
dereferencing it would require to get the data location from the control block
first.

.. _make-shared:

.. image:: make_shared.png
   :alt: Memory layout when using ``std::make_shared``

When using ``make_shared`` or ``allocate_shared``, the standard
library can store reference count and data in a single memory block as shown
above. However, ``shared_ptr`` still has to store two pointers, because
it is unaware where the data is allocated.

.. _enable-shared-from-this:

.. image:: enable_shared_from_this.png
   :alt: Memory layout with ``std::enable_shared_from_this``

Finally, the design of the standard library becomes convoluted when an object
should be able to hand out a ``shared_ptr`` to itself. Classes must
inherit from ``std::enable_shared_from_this`` to navigate from an
object to its control block. This additional navigation path is required,
because ``std::shared_ptr`` needs two pointers. One to the data and one
to the control block. Programmers can still use ``make_shared`` for
such objects, in which case the object is again stored along with the control
block.

Smart Pointers to Actors
------------------------

In CAF, we use a different approach than the standard library because (1) we
always allocate actors along with their control block, (2) we need additional
information in the control block, and (3) we can store only a single raw
pointer internally instead of the two raw pointers ``std::shared_ptr``
needs. The following figure summarizes the design of smart pointers to actors.

.. image:: refcounting.png
   :alt: Shared pointer design in CAF

CAF uses ``strong_actor_ptr`` instead of
``std::shared_ptr<...>`` and ``weak_actor_ptr`` instead of
``std::weak_ptr<...>``. Unlike the counterparts from the standard
library, both smart pointer types only store a single pointer.

Also, the control block in CAF is not a template and stores the identity of an
actor (``actor_id`` plus ``node_id``). This allows CAF to
access this information even after an actor died. The control block fits
exactly into a single cache line (64 Bytes). This makes sure no *false
sharing* occurs between an actor and other actors that have references to it.
Since the size of the control block is fixed and CAF *guarantees* the
memory layout enforced by ``actor_storage``, CAF can compute the
address of an actor from the pointer to its control block by offsetting it by
64 Bytes. Likewise, an actor can compute the address of its control block.

The smart pointer design in CAF relies on a few assumptions about actor types.
Most notably, the actor object is placed 64 Bytes after the control block. This
starting address is cast to ``abstract_actor*``. Hence, ``T*``
must be convertible to ``abstract_actor*`` via
``reinterpret_cast``. In practice, this means actor subclasses must not
use virtual inheritance, which is enforced in CAF with a
``static_assert``.

Strong and Weak References
--------------------------

A *strong* reference manipulates the ``strong refs`` counter as shown above. An
actor is destroyed if there are *zero* strong references to it. If two actors
keep strong references to each other via member variable, neither actor can ever
be destroyed because they produce a cycle (see :ref:`breaking-cycles`). Strong
references are formed by ``strong_actor_ptr``, ``actor``, and
``typed_actor<...>`` (see :ref:`actor-reference`).

A *weak* reference manipulates the ``weak refs`` counter. This counter keeps
track of how many references to the control block exist. The control block is
destroyed if there are *zero* weak references to an actor (which cannot occur
before ``strong refs`` reached *zero* as well). No cycle occurs if two actors
keep weak references to each other, because the actor objects themselves can get
destroyed independently from their control block.  A weak reference is only
formed by ``actor_addr`` (see :ref:`actor-address`).

.. _actor-cast:

Converting Actor References with ``actor_cast``
-----------------------------------------------

The function ``actor_cast`` converts between actor pointers and
handles. The first common use case is to convert a ``strong_actor_ptr``
to either ``actor`` or ``typed_actor<...>`` before being able
to send messages to an actor. The second common use case is to convert
``actor_addr`` to ``strong_actor_ptr`` to upgrade a weak
reference to a strong reference. Note that casting ``actor_addr`` to a
strong actor pointer or handle can result in invalid handles. The syntax for
``actor_cast`` resembles builtin C++ casts. For example,
``actor_cast<actor>(x)`` converts ``x`` to an handle of type
``actor``.

.. _breaking-cycles:

Breaking Cycles Manually
------------------------

Cycles can occur only when using class-based actors when storing references to
other actors via member variable. Stateful actors (see :ref:`stateful-actor`)
break cycles by destroying the state when an actor terminates, *before* the
destructor of the actor itself runs. This means an actor releases all references
to others automatically after calling ``quit``. However, class-based actors have
to break cycles manually, because references to others are not released until
the destructor of an actor runs. Two actors storing references to each other via
member variable produce a cycle and neither destructor can ever be called.

Class-based actors can break cycles manually by overriding ``on_exit()`` and
calling ``destroy(x)`` on each handle (see :ref:`actor-handle`). Using a handle
after destroying it is undefined behavior, but it is safe to assign a new value
to the handle.
