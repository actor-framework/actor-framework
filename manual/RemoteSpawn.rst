.. _remote-spawn:

Remote Spawning of Actors
=========================

Remote spawning is an extension of the dynamic spawn using run-time type names
(see :ref:`add-custom-actor-type`). The following example assumes a typed actor
handle named ``calculator`` with an actor implementing this messaging interface
named "calculator".

.. literalinclude:: /examples/remoting/remote_spawn.cpp
   :language: C++
   :start-after: --(rst-client-begin)--
   :end-before: --(rst-client-end)--

We first connect to a CAF node with ``middleman().connect(...)``. On success,
``connect`` returns the node ID we need for ``remote_spawn``. This requires the
server to open a port with ``middleman().open(...)`` or
``middleman().publish(...)``. Alternatively, we can obtain the node ID from an
already existing remote actor handle---returned from ``remote_actor`` for
example---via ``hdl->node()``. After connecting to the server, we can use
``middleman().remote_spawn<...>(...)`` to create actors remotely.
