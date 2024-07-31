.. _typed-message-view:

Typed Message View
==================

For the most part, users can write CAF applications without knowing about
``message``. When sending messages between actors, CAF automatically wraps the
content for the message into this type-erased container and then matches each
incoming message against the message handlers.

However, there are use cases for ``message`` outside of the dispatching logic in
CAF. For example, actors can store messages that they currently don't want to
(or cannot) process in some cache for processing them later. Or an application
could read messages from custom data sources and then deploy on their run-time
type.

For such use cases, CAF includes typed message views. On construction, a view
performs the necessary type checking. On a match, users can then query
individual elements from the view via ``get<Index>(x)``, much like the interface
offered by ``std::tuple``.

Internally, messages use copy-on-write (see :ref:`copy-on-write-types`). Hence,
there are two flavors of typed message views: ``const`` views that represent
read-only access and views with mutable access. The latter forces the message to
perform a deep copy of its data if there is more to one reference to the data.
To cut a long story short, we recommend sticking to the ``const`` version
whenever possible.

For both view types, the simplest way of using them is to construct a view
object and then check whether it is valid. The ``const`` version is called
``const_typed_message_view``:

.. code-block:: C++

  auto msg1 = caf::make_message("hello", "world");
  auto msg2 = msg1;
  if (auto v = caf::const_typed_message_view<std::string, std::string>{msg1})
    sys.println("v: {}, {}", caf::get<0>(v), caf::get<1>(v));
  // Both messages still point to the same data.
  assert(msg1.cptr() == msg2.cptr());

The mutable version is simply called ``typed_message_view``, but otherwise has a
similar interface:

.. code-block:: C++

  auto msg1 = caf::make_message("hello", "world");
  auto msg2 = msg1;
  if (auto v = caf::typed_message_view<std::string, std::string>{msg1}) {
    caf::get<0>(v) = "bye";
    sys.println("v: {}, {}", caf::get<0>(v), caf::get<1>(v));
  }
  // The messages no longer point to the same data.
  assert(msg1.cptr() != msg2.cptr());
