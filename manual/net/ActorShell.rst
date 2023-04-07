.. include:: vars.rst

.. _net-actor-shell:

Actor Shell
===========

Actor shells allow socket managers to interface with regular actors. Much like
regular actors, actor shells come in two flavors: with dynamic typing
(``actor_shell``) or with static typing (``typed_actor_shell``).

Actor shells can be embedded into a protocol instance to turn messages on the
network to actor messages and vice versa. The primary use case in CAF at the
moment is to allow servers to send a request message to an actor and then use
the response message to generate an output on the network. Please see
``examples/http/rest.cpp`` as a reference for this use case.

Unlike a "regular" actor, an actor shell has no own control loop. Users can
define a behavior with ``set_behavior``, but are responsible for embedding the
shell into some sort of control loop.

|see-doxygen|
