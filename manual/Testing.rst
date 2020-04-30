.. _testing:

Testing
=======

CAF comes with built-in support for writing unit tests in a domain-specific
language (DSL). The API looks similar to well-known testing frameworks such as
Boost.Test and Catch but adds CAF-specific macros for testing messaging between
actors.

Our design leverages four main concepts:

* **Checks** represent single boolean expressions.
* **Tests** contain one or more checks.
* **Fixtures** equip tests with a fixed data environment.
* **Suites** group tests together.

The following code illustrates a very basic test case that captures the four
main concepts described above.

.. code-block:: C++

   // Adds all tests in this compilation unit to the suite "math".
   #define CAF_SUITE math

   // Pulls in all the necessary macros.
   #include "caf/test/dsl.hpp"

   namespace {

   struct fixture {};

   } // namespace

   // Makes all members of `fixture` available to tests in the scope.
   CAF_TEST_FIXTURE_SCOPE(math_tests, fixture)

   // Implements our first test.
   CAF_TEST(divide) {
     CAF_CHECK(1 / 1 == 0); // fails
     CAF_CHECK(2 / 2 == 1); // passes
     CAF_REQUIRE(3 + 3 == 5); // fails and aborts test execution
     CAF_CHECK(4 - 4 == 0); // unreachable due to previous requirement error
   }

   CAF_TEST_FIXTURE_SCOPE_END()

The code above highlights the two basic macros ``CAF_CHECK`` and
``CAF_REQUIRE``. The former reports failed checks, but allows the test
to continue on error. The latter stops test execution if the boolean expression
evaluates to false.

The third macro worth mentioning is ``CAF_FAIL``. It unconditionally
stops test execution with an error message. This is particularly useful for
stopping program execution after receiving unexpected messages, as we will see
later.

Testing Actors
--------------

The following example illustrates how to add an actor system as well as a
scoped actor to fixtures. This allows spawning of and interacting with actors
in a similar way regular programs would. Except that we are using macros such
as ``CAF_CHECK`` and provide tests rather than implementing a
``caf_main``.

.. code-block:: C++

   namespace {

   struct fixture {
     caf::actor_system_config cfg;
     caf::actor_system sys;
     caf::scoped_actor self;

     fixture() : sys(cfg), self(sys) {
       // nop
     }
   };

   caf::behavior adder() {
     return {
       [=](int x, int y) {
         return x + y;
       }
     };
   }

   } // namespace

   CAF_TEST_FIXTURE_SCOPE(actor_tests, fixture)

   CAF_TEST(simple actor test) {
     // Our Actor-Under-Test.
     auto aut = self->spawn(adder);
     self->request(aut, caf::infinite, 3, 4).receive(
       [=](int r) {
         CAF_CHECK(r == 7);
       },
       [&](caf::error& err) {
         // Must not happen, stop test.
         CAF_FAIL(err);
       });
   }

   CAF_TEST_FIXTURE_SCOPE_END()

The example above works, but suffers from several issues:

* Significant amount of boilerplate code.
* Using a scoped actor as illustrated above can only test one actor at a  time. However, messages between other actors are invisible to us.
* CAF runs actors in a thread pool by default. The resulting nondeterminism  makes triggering reliable ordering of messages near impossible. Further,  forcing timeouts to test error handling code is even harder.

Deterministic Testing
---------------------

CAF provides a scheduler implementation specifically tailored for writing unit
tests called ``test_coordinator``. It does not start any threads and
instead gives unit tests full control over message dispatching and timeout
management.

To reduce boilerplate code, CAF also provides a fixture template called
``test_coordinator_fixture`` that comes with ready-to-use actor system
(``sys``) and testing scheduler (``sched``). The optional
template parameter allows unit tests to plugin custom actor system
configuration classes.

Using this fixture unlocks three additional macros:

* ``expect`` checks for a single message. The macro verifies the  content types of the message and invokes the necessary member functions on  the test coordinator. Optionally, the macro checks the receiver of the  message and its content. If the expected message does not exist, the test  aborts.
* ``allow`` is similar to ``expect``, but it does not abort  the test if the expected message is missing. This macro returns  ``true`` if the allowed message was delivered, ``false``  otherwise.
* ``disallow`` aborts the test if a particular message was delivered  to an actor.

The following example implements two actors, ``ping`` and
``pong``, that exchange a configurable amount of messages. The test
*three pings* then checks the contents of each message with
``expect`` and verifies that no additional messages exist using
``disallow``.

.. literalinclude:: /examples/testing/ping_pong.cpp
   :language: C++
   :lines: 12-60


