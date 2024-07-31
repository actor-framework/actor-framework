.. _worker-set:

.. note::

  This is a commercial feature and requires a software license from Interance.
  Please visit https://www.interance.io for more information and pricing.

Worker Set
==========

To use this class in your application, include the header
``interance/worker_set.hpp``.

A worker set manages a collection of actors that distributes work among its
members. The set forwards incoming messages to the next available worker. If no
worker is available, the set caches the message until a worker becomes idle.
This pattern is useful for load balancing and parallelizing workloads.

The worker set itself is an actor that manages a pool of homogenous workers.
Sending a message to the set immediately forwards it to the next available
worker. This delegation is done immediately when enqueueing the message, i.e.,
the message will be transparently enqueued directly into the mailbox of an idle
worker. The set caches messages only if no worker is available.

Building a Worker Set
---------------------

The entrypoint for creating a worker set is the
``interance::worker_set_builder`` class. While users can construct instances of
this class directly, we recommend using the factory function
``interance::new_worker_set``. It will return a builder object that then allows
users to configure the worker set before calling ``make``.

The builder object allows users to configure the following properties:

``max_restarts: size_t``
  The maximum number of restarts allowed within the given period.

``period: caf::timespan``
  The time period in which ``max_restarts`` are allowed. For example, setting
  this to 5 seconds and ``max_restarts`` to 10 means that the set restarts
  workers at most 10 times within 5 seconds. If a worker fails for the 11th time
  within this period, the entire set and all of its workers will terminate.

``name: std::string``
  The name of the worker set actor. Setting a custom name is useful for logging
  and debugging.

``detach_workers``
  Setting this flag configures the set to detach its workers, i.e., spawns each
  worker in its own thread. Using this option essentially turns the worker set
  into a thread pool.

``on_worker_restart: callback``
  The signature for this callback is ``void (caf::actor_addr, caf::actor)``,
  whereas the first parameter is the address of the failed worker and the second
  parameter is the handle to the new worker replacing it. Setting this callback
  allows users to react to worker restarts. For example, users can log the event
  or use the callback to generate additional telemetry data.

To configure a property, users can call the corresponding member function on the
builder object with an appropriate argument (except ``detach_workers``, which
takes no argument).  For example, to create a worker set named ``my-worker-set``
that spawns workers of type ``worker_impl`` with a maximum of 5 restarts within
10 seconds, users can write:

.. code-block:: C++

  auto ws = interance::new_worker_set(sys)
              .name("my-worker-set")
              .max_restarts(5)
              .period(10s)
              .make(worker_impl);

The Worker Set Class
--------------------

The worker set class only provides a hand full of member functions.

``caf::actor as_actor() const``
  Returns the actor handle of the worker set. This handle can be used to send
  messages to the set, which in turn will forward them to the workers. Usually,
  the worker set object itself can be discarded after starting workers on the
  set and obtaining the actor handle.

``size_t num_workers() const``
  Returns the current number of workers in the set. Note that this number can
  change asynchronously due to worker restarts.

``worker_set& add(size_t num)``
  Adds ``num`` workers to the set and returns a reference to the set.

``std::vector<caf::actor> worker_handles()``
  Returns the handles of all workers in the set that are currently running.

A usual work flow for creating a worker set looks like this:

.. code-block:: C++

  auto worker_impl = [] {
    return caf::behavior{
      [](int value) {
        return value * 2;
      },
    };
  };

  auto ws = interance::new_worker_set(sys)
              .name("my-worker-set")
              .max_restarts(5)
              .period(10s)
              .make(worker_impl)
              .add(8)
              .as_actor();

  caf::scoped_actor self{sys};
  self->mail(21).request(ws, 1s).receive(
    [&self](int result) {
      self->println("21 -> {}", result);
    },
    [&self](const caf::error& err) {
      self->println("error: {}", err);
    }
  );

Once we have added workers to the set, we can obtain the actor handle and start
sending messages to it. The ``worker_set`` handle itself is usually discarded
after the setup phase.
