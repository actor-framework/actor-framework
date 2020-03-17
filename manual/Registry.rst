.. _registry:

Registry
========

The actor registry in CAF keeps track of the number of running actors and allows
to map actors to their ID or a custom atom (see :ref:`atom`) representing a
name. The registry does *not* contain all actors. Actors have to be stored in
the registry explicitly. Users can access the registry through an actor system
by calling ``system.registry()``. The registry stores actors using
``strong_actor_ptr`` (see :ref:`actor-pointer`).

Users can use the registry to make actors system-wide available by name. The
:ref:`middleman` uses the registry to keep track of all actors known to remote
nodes in order to serialize and deserialize them. Actors are removed
automatically when they terminate.

It is worth mentioning that the registry is not synchronized between connected
actor system. Each actor system has its own, local registry in a distributed
setting.

+-------------------------------------------+-------------------------------------------------+
| **Types**                                 |                                                 |
+-------------------------------------------+-------------------------------------------------+
| ``name_map``                              | ``unordered_map<atom_value, strong_actor_ptr>`` |
+-------------------------------------------+-------------------------------------------------+
|                                           |                                                 |
+-------------------------------------------+-------------------------------------------------+
| **Observers**                             |                                                 |
+-------------------------------------------+-------------------------------------------------+
| ``strong_actor_ptr get(actor_id)``        | Returns the actor associated to given ID.       |
+-------------------------------------------+-------------------------------------------------+
| ``strong_actor_ptr get(atom_value)``      | Returns the actor associated to given name.     |
+-------------------------------------------+-------------------------------------------------+
| ``name_map named_actors()``               | Returns all name mappings.                      |
+-------------------------------------------+-------------------------------------------------+
| ``size_t running()``                      | Returns the number of currently running actors. |
+-------------------------------------------+-------------------------------------------------+
|                                           |                                                 |
+-------------------------------------------+-------------------------------------------------+
| **Modifiers**                             |                                                 |
+-------------------------------------------+-------------------------------------------------+
| ``void put(actor_id, strong_actor_ptr)``  | Maps an actor to its ID.                        |
+-------------------------------------------+-------------------------------------------------+
| ``void erase(actor_id)``                  | Removes an ID mapping from the registry.        |
+-------------------------------------------+-------------------------------------------------+
| ``void put(atom_value, strong_actor_ptr)``| Maps an actor to a name.                        |
+-------------------------------------------+-------------------------------------------------+
| ``void erase(atom_value)``                | Removes a name mapping from the registry.       |
+-------------------------------------------+-------------------------------------------------+
