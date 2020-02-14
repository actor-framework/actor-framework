\section{Middleman}
\label{middleman}

The middleman is the main component of the I/O module and enables distribution.
It transparently manages proxy actor instances representing remote actors,
maintains connections to other nodes, and takes care of serialization of
messages. Applications install a middleman by loading
\lstinline^caf::io::middleman^ as module~\see{system-config}. Users can include
\lstinline^"caf/io/all.hpp"^ to get access to all public classes of the I/O
module.

\subsection{Class \texttt{middleman}}

\begin{center}
\begin{tabular}{ll}
  \textbf{Remoting} & ~ \\
  \hline
  \lstinline^expected<uint16> open(uint16, const char*, bool)^ & See~\sref{remoting}. \\
  \hline
  \lstinline^expected<uint16> publish(T, uint16, const char*, bool)^ & See~\sref{remoting}. \\
  \hline
  \lstinline^expected<void> unpublish(T x, uint16)^ & See~\sref{remoting}. \\
  \hline
  \lstinline^expected<node_id> connect(std::string host, uint16_t port)^ & See~\sref{remoting}. \\
  \hline
  \lstinline^expected<T> remote_actor<T = actor>(string, uint16)^ & See~\sref{remoting}. \\
  \hline
  \lstinline^expected<T> spawn_broker(F fun, ...)^ & See~\sref{broker}. \\
  \hline
  \lstinline^expected<T> spawn_client(F, string, uint16, ...)^ & See~\sref{broker}. \\
  \hline
  \lstinline^expected<T> spawn_server(F, uint16, ...)^ & See~\sref{broker}. \\
  \hline
\end{tabular}
\end{center}

\subsection{Publishing and Connecting}
\label{remoting}

The member function \lstinline^publish^ binds an actor to a given port, thereby
allowing other nodes to access it over the network.

\begin{lstlisting}
template <class T>
expected<uint16_t> middleman::publish(T x, uint16_t port,
                                      const char* in = nullptr,
                                      bool reuse_addr = false);
\end{lstlisting}

The first argument is a handle of type \lstinline^actor^ or
\lstinline^typed_actor<...>^. The second argument denotes the TCP port. The OS
will pick a random high-level port when passing 0. The third parameter
configures the listening address. Passing null will accept all incoming
connections (\lstinline^INADDR_ANY^). Finally, the flag \lstinline^reuse_addr^
controls the behavior when binding an IP address to a port, with the same
semantics as the BSD socket flag \lstinline^SO_REUSEADDR^. For example, with
\lstinline^reuse_addr = false^, binding two sockets to 0.0.0.0:42 and
10.0.0.1:42 will fail with \texttt{EADDRINUSE} since 0.0.0.0 includes 10.0.0.1.
With \lstinline^reuse_addr = true^ binding would succeed because 10.0.0.1 and
0.0.0.0 are not literally equal addresses.

The member function returns the bound port on success. Otherwise, an
\lstinline^error^ \see{error} is returned.

\begin{lstlisting}
template <class T>
expected<uint16_t> middleman::unpublish(T x, uint16_t port = 0);
\end{lstlisting}

The member function \lstinline^unpublish^ allows actors to close a port
manually. This is performed automatically if the published actor terminates.
Passing 0 as second argument closes all ports an actor is published to,
otherwise only one specific port is closed.

The function returns an \lstinline^error^ \see{error} if the actor was not
bound to given port.

\clearpage
\begin{lstlisting}
template<class T = actor>
expected<T> middleman::remote_actor(std::string host, uint16_t port);
\end{lstlisting}

After a server has published an actor with \lstinline^publish^, clients can
connect to the published actor by calling \lstinline^remote_actor^:

\begin{lstlisting}
// node A
auto ping = spawn(ping);
system.middleman().publish(ping, 4242);

// node B
auto ping = system.middleman().remote_actor("node A", 4242);
if (! ping) {
  cerr << "unable to connect to node A: "
       << system.render(ping.error()) << std::endl;
} else {
  self->send(*ping, ping_atom::value);
}
\end{lstlisting}

There is no difference between server and client after the connection phase.
Remote actors use the same handle types as local actors and are thus fully
transparent.

The function pair \lstinline^open^ and \lstinline^connect^ allows users to
connect CAF instances without remote actor setup. The function
\lstinline^connect^ returns a \lstinline^node_id^ that can be used for remote
spawning (see~\sref{remote-spawn}).

\subsection{Free Functions}
\label{free-remoting-functions}

The following free functions in the namespace \lstinline^caf::io^ avoid calling
the middleman directly. This enables users to easily switch between
communication backends as long as the interfaces have the same signatures. For
example, the (experimental) OpenSSL binding of CAF implements the same
functions in the namespace \lstinline^caf::openssl^ to easily switch between
encrypted and unencrypted communication.

\begin{center}
\begin{tabular}{ll}
  \hline
  \lstinline^expected<uint16> open(actor_system&, uint16, const char*, bool)^ & See~\sref{remoting}. \\
  \hline
  \lstinline^expected<uint16> publish(T, uint16, const char*, bool)^ & See~\sref{remoting}. \\
  \hline
  \lstinline^expected<void> unpublish(T x, uint16)^ & See~\sref{remoting}. \\
  \hline
  \lstinline^expected<node_id> connect(actor_system&, std::string host, uint16_t port)^ & See~\sref{remoting}. \\
  \hline
  \lstinline^expected<T> remote_actor<T = actor>(actor_system&, string, uint16)^ & See~\sref{remoting}. \\
  \hline
\end{tabular}
\end{center}

\subsection{Transport Protocols \experimental}
\label{transport-protocols}

CAF communication uses TCP per default and thus the functions shown in the
middleman API above are related to TCP. There are two alternatives to plain
TCP: TLS via the OpenSSL module shortly discussed in
\sref{free-remoting-functions} and UDP.

UDP is integrated in the default multiplexer and BASP broker. Set the flag
\lstinline^middleman_enable_udp^ to true to enable it
(see~\sref{system-config}). This does not require you to disable TCP. Use
\lstinline^publish_udp^ and \lstinline^remote_actor_udp^ to establish
communication.

Communication via UDP is inherently unreliable and unordered. CAF reestablishes
order and drops messages that arrive late. Messages that are sent via datagrams
are limited to a maximum of 65.535 bytes which is used as a receive buffer size
by CAF. Note that messages that exceed the MTU are fragmented by IP and are
considered lost if a single fragment is lost. Optional reliability based on
retransmissions and messages slicing on the application layer are planned for
the future.
