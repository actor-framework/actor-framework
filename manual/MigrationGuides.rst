\section{Migration Guides}

The guides in this section document all possibly breaking changes in the
library for that last versions of CAF.

\subsection{0.8 to 0.9}

Version 0.9 included a lot of changes and improvements in its implementation,
but it also made breaking changes to the API.

\paragraph{\lstinline^self^ has been removed}

~

This is the biggest library change since the initial release. The major problem
with this keyword-like identifier is that it must have a single type as it's
implemented as a thread-local variable. Since there are so many different kinds
of actors (event-based or blocking, untyped or typed), \lstinline^self^ needs
to perform type erasure at some point, rendering it ultimately useless. Instead
of a thread-local pointer, you can now use the first argument in functor-based
actors to "catch" the self pointer with proper type information.

\paragraph{\lstinline^actor_ptr^ has been replaced}

~

CAF now distinguishes between handles to actors, i.e.,
\lstinline^typed_actor<...>^ or simply \lstinline^actor^, and \emph{addresses}
of actors, i.e., \lstinline^actor_addr^. The reason for this change is that
each actor has a logical, (network-wide) unique address, which is used by the
networking layer of CAF. Furthermore, for monitoring or linking, the address
is all you need. However, the address is not sufficient for sending messages,
because it doesn't have any type information. The function
\lstinline^current_sender()^ now returns the \emph{address} of the sender. This
means that previously valid code such as \lstinline^send(current_sender(),...)^
will cause a compiler error. However, the recommended way of replying to
messages is to return the result from the message handler.

\paragraph{The API for typed actors is now similar to the API for untyped actors}

~

The APIs of typed and untyped actors have been harmonized. Typed actors can now
be published in the network and also use all operations untyped actors can.

\clearpage
\subsection{0.9 to 0.10 (\texttt{libcppa} to CAF)}

The first release under the new name CAF is an overhaul of the entire library.
Some classes have been renamed or relocated, others have been removed. The
purpose of this refactoring was to make the library easier to grasp and to make
its API more consistent. All classes now live in the namespace \texttt{caf} and
all headers have the top level folder \texttt{caf} instead of \texttt{cppa}.
For example, \texttt{cppa/actor.hpp} becomes \texttt{caf/actor.hpp}. Further,
the convenience header to get all parts of the user API is now
\texttt{"caf/all.hpp"}. The networking has been separated from the core
library. To get the networking components, simply include
\texttt{caf/io/all.hpp} and use the namespace \lstinline^caf::io^.

Version 0.10 still includes the header \texttt{cppa/cppa.hpp} to make the
transition process for users easier and to not break existing code right away.
The header defines the namespace \texttt{cppa} as an alias for \texttt{caf}.
Furthermore, it provides implementations or type aliases for renamed or removed
classes such as \lstinline^cow_tuple^. You won't get any warning about deprecated
headers with 0.10. However, we will add this warnings in the next library
version and remove deprecated code eventually.

Even when using the backwards compatibility header, the new library has
breaking changes. For instance, guard expressions have been removed entirely.
The reasoning behind this decision is that we already have projections to
modify the outcome of a match. Guard expressions add little expressive power to
the library but a whole lot of code that is hard to maintain in the long run
due to its complexity. Using projections to not only perform type conversions
but also to restrict values is the more natural choice.

\lstinline^any_tuple => message^

This type is only being used to pass a message from one actor to another.
Hence, \lstinline^message^ is the logical name.

\lstinline^partial_function => message_handler^

Technically, it still is a partial function. However, we wanted to put
emphasize on its use case.

\lstinline^cow_tuple => X^

We want to provide a streamlined, simple API. Shipping a full tuple abstraction
with the library does not fit into this philosophy. The removal of
\lstinline^cow_tuple^ implies the removal of related functions such as
\lstinline^tuple_cast^.

\lstinline^cow_ptr => X^

This pointer class is an implementation detail of \lstinline^message^ and
should not live in the global namespace in the first place. It also had the
wrong name, because it is intrusive.

\lstinline^X => message_builder^

This new class can be used to create messages dynamically. For example, the
content of a vector can be used to create a message using a series of
\lstinline^append^ calls.

\begin{lstlisting}
accept_handle, connection_handle, publish, remote_actor,
max_msg_size, typed_publish, typed_remote_actor, publish_local_groups,
new_connection_msg, new_data_msg, connection_closed_msg, acceptor_closed_msg
\end{lstlisting}

These classes concern I/O functionality and have thus been moved to
\lstinline^caf::io^

\subsection{0.10 to 0.11}

Version 0.11 introduced new, optional components. The core library itself,
however, mainly received optimizations and bugfixes with one exception: the
member function \lstinline^on_exit^ is no longer virtual. You can still provide
it to define a custom exit handler, but you must not use \lstinline^override^.

\subsection{0.11 to 0.12}

Version 0.12 removed two features:

\begin{itemize}
\item Type names are no longer demangled automatically. Hence, users must
  explicitly pass the type name as first argument when using
    \lstinline^announce^, i.e., \lstinline^announce<my_class>(...)^ becomes
    \lstinline^announce<my_class>("my_class", ...)^.
\item Synchronous send blocks no longer support \lstinline^continue_with^. This
  feature has been removed without substitution.
\end{itemize}

\subsection{0.12 to 0.13}

This release removes the (since 0.9 deprecated) \lstinline^cppa^ headers and
deprecates all \lstinline^*_send_tuple^ versions (simply use the function
without \lstinline^_tuple^ suffix). \lstinline^local_actor::on_exit^ once again
became virtual.

In case you were using the old \lstinline^cppa::options_description^ API, you
can migrate to the new API based on \lstinline^extract^ \see{extract-opts}.

Most importantly, version 0.13 slightly changes \lstinline^last_dequeued^ and
\lstinline^last_sender^. Both functions will now cause undefined behavior
(dereferencing a \lstinline^nullptr^) instead of returning dummy values when
accessed from outside a callback or after forwarding the current message.
Besides, these function names were not a good choice in the first place, since
``last'' implies accessing data received in the past. As a result, both
functions are now deprecated. Their replacements are named
\lstinline^current_message^ and \lstinline^current_sender^ \see{interface}.

\subsection{0.13 to 0.14}

The function \lstinline^timed_sync_send^ has been removed. It offered an
alternative way of defining message handlers, which is inconsistent with the
rest of the API.

The policy classes \lstinline^broadcast^, \lstinline^random^, and
\lstinline^round_robin^ in \lstinline^actor_pool^ were removed and replaced by
factory functions using the same name.

\clearpage
\subsection{0.14 to 0.15}

Version 0.15 replaces the singleton-based architecture with
\lstinline^actor_system^. Most of the free functions in namespace
\lstinline^caf^ are now member functions of \lstinline^actor_system^
\see{actor-system}. Likewise, most functions in namespace \lstinline^caf::io^
are now member functions of \lstinline^middleman^ \see{middleman}. The
structure of CAF applications has changed fundamentally with a focus on
configurability. Setting and fine-tuning the scheduler, changing parameters of
the middleman, etc. is now bundled in the class
\lstinline^actor_system_config^. The new configuration mechanism is also easily
extensible.

Patterns are now limited to the simple notation, because the advanced features
(1) are not implementable for statically typed actors, (2) are not portable to
Windows/MSVC, and (3) drastically impact compile times. Dropping this
functionality also simplifies the implementation and improves performance.

The \lstinline^blocking_api^ flag has been removed. All variants of
\emph{spawn} now auto-detect blocking actors.
