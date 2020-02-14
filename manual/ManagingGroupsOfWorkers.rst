\section{Managing Groups of Workers \experimental}
\label{worker-groups}

When managing a set of workers, a central actor often dispatches requests to a
set of workers. For this purpose, the class \lstinline^actor_pool^ implements a
lightweight abstraction for managing a set of workers using a dispatching
policy. Unlike groups, pools usually own their workers.

Pools are created using the static member function \lstinline^make^, which
takes either one argument (the policy) or three (number of workers, factory
function for workers, and dispatching policy). After construction, one can add
new workers via messages of the form \texttt{('SYS', 'PUT', worker)}, remove
workers with \texttt{('SYS', 'DELETE', worker)}, and retrieve the set of
workers as \lstinline^vector<actor>^ via \texttt{('SYS', 'GET')}.

An actor pool takes ownership of its workers. When forced to quit, it sends an
exit messages to all of its workers, forcing them to quit as well. The pool
also monitors all of its workers.

Pools do not cache messages, but enqueue them directly in a workers mailbox.
Consequently, a terminating worker loses all unprocessed messages. For more
advanced caching strategies, such as reliable message delivery, users can
implement their own dispatching policies.

\subsection{Dispatching Policies}

A dispatching policy is a functor with the following signature:

\begin{lstlisting}
using uplock = upgrade_lock<detail::shared_spinlock>;
using policy = std::function<void (actor_system& sys,
                                   uplock& guard,
                                   const actor_vec& workers,
                                   mailbox_element_ptr& ptr,
                                   execution_unit* host)>;
\end{lstlisting}

The argument \lstinline^guard^ is a shared lock that can be upgraded for unique
access if the policy includes a critical section. The second argument is a
vector containing all workers managed by the pool. The argument \lstinline^ptr^
contains the full message as received by the pool. Finally, \lstinline^host^ is
the current scheduler context that can be used to enqueue workers into the
corresponding job queue.

The actor pool class comes with a set predefined policies, accessible via
factory functions, for convenience.

\begin{lstlisting}
actor_pool::policy actor_pool::round_robin();
\end{lstlisting}

This policy forwards incoming requests in a round-robin manner to workers.
There is no guarantee that messages are consumed, i.e., work items are lost if
the worker exits before processing all of its messages.

\begin{lstlisting}
actor_pool::policy actor_pool::broadcast();
\end{lstlisting}

This policy forwards \emph{each} message to \emph{all} workers. Synchronous
messages to the pool will be received by all workers, but the client will only
recognize the first arriving response message---or error---and discard
subsequent messages. Note that this is not caused by the policy itself, but a
consequence of forwarding synchronous messages to more than one actor.

\begin{lstlisting}
actor_pool::policy actor_pool::random();
\end{lstlisting}

This policy forwards incoming requests to one worker from the pool chosen
uniformly at random. Analogous to \lstinline^round_robin^, this policy does not
cache or redispatch messages.

\begin{lstlisting}
using join = function<void (T&, message&)>;
using split = function<void (vector<pair<actor, message>>&, message&)>;
template <class T>
static policy split_join(join jf, split sf = ..., T init = T());
\end{lstlisting}

This policy models split/join or scatter/gather work flows, where a work item
is split into as many tasks as workers are available and then the individuals
results are joined together before sending the full result back to the client.

The join function is responsible for ``glueing'' all result messages together
to create a single result. The function is called with the result object
(initialed using \lstinline^init^) and the current result messages from a
worker.

The first argument of a split function is a mapping from actors (workers) to
tasks (messages). The second argument is the input message. The default split
function is a broadcast dispatching, sending each worker the original request.
