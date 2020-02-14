\section{Testing}
\label{testing}

CAF comes with built-in support for writing unit tests in a domain-specific
language (DSL). The API looks similar to well-known testing frameworks such as
Boost.Test and Catch but adds CAF-specific macros for testing messaging between
actors.

Our design leverages four main concepts:

\begin{itemize}
    \item \textbf{Checks} represent single boolean expressions.
    \item \textbf{Tests} contain one or more checks.
    \item \textbf{Fixtures} equip tests with a fixed data environment.
    \item \textbf{Suites} group tests together.
\end{itemize}

The following code illustrates a very basic test case that captures the four
main concepts described above.

\begin{lstlisting}
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
\end{lstlisting}

The code above highlights the two basic macros \lstinline^CAF_CHECK^ and
\lstinline^CAF_REQUIRE^. The former reports failed checks, but allows the test
to continue on error. The latter stops test execution if the boolean expression
evaluates to false.

The third macro worth mentioning is \lstinline^CAF_FAIL^. It unconditionally
stops test execution with an error message. This is particularly useful for
stopping program execution after receiving unexpected messages, as we will see
later.

\subsection{Testing Actors}

The following example illustrates how to add an actor system as well as a
scoped actor to fixtures. This allows spawning of and interacting with actors
in a similar way regular programs would. Except that we are using macros such
as \lstinline^CAF_CHECK^ and provide tests rather than implementing a
\lstinline^caf_main^.

\begin{lstlisting}
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
      CAF_FAIL(sys.render(err));
    });
}

CAF_TEST_FIXTURE_SCOPE_END()
\end{lstlisting}

The example above works, but suffers from several issues:

\begin{itemize}

  \item

    Significant amount of boilerplate code.

  \item

    Using a scoped actor as illustrated above can only test one actor at a
    time. However, messages between other actors are invisible to us.

  \item

    CAF runs actors in a thread pool by default. The resulting nondeterminism
    makes triggering reliable ordering of messages near impossible. Further,
    forcing timeouts to test error handling code is even harder.

\end{itemize}

\subsection{Deterministic Testing}

CAF provides a scheduler implementation specifically tailored for writing unit
tests called \lstinline^test_coordinator^. It does not start any threads and
instead gives unit tests full control over message dispatching and timeout
management.

To reduce boilerplate code, CAF also provides a fixture template called
\lstinline^test_coordinator_fixture^ that comes with ready-to-use actor system
(\lstinline^sys^) and testing scheduler (\lstinline^sched^). The optional
template parameter allows unit tests to plugin custom actor system
configuration classes.

Using this fixture unlocks three additional macros:

\begin{itemize}

  \item

    \lstinline^expect^ checks for a single message. The macro verifies the
    content types of the message and invokes the necessary member functions on
    the test coordinator. Optionally, the macro checks the receiver of the
    message and its content. If the expected message does not exist, the test
    aborts.

  \item

    \lstinline^allow^ is similar to \lstinline^expect^, but it does not abort
    the test if the expected message is missing. This macro returns
    \lstinline^true^ if the allowed message was delivered, \lstinline^false^
    otherwise.

  \item

    \lstinline^disallow^ aborts the test if a particular message was delivered
    to an actor.

\end{itemize}

The following example implements two actors, \lstinline^ping^ and
\lstinline^pong^, that exchange a configurable amount of messages. The test
\emph{three pings} then checks the contents of each message with
\lstinline^expect^ and verifies that no additional messages exist using
\lstinline^disallow^.

\cppexample[12-65]{testing/ping_pong}
