\section{Group Communication}
\label{groups}

CAF supports publish/subscribe-based group communication. Dynamically typed
actors can join and leave groups and send messages to groups. The following
example showcases the basic API for retrieving a group from a module by its
name, joining, and leaving.

\begin{lstlisting}
std::string module = "local";
std::string id = "foo";
auto expected_grp = system.groups().get(module, id);
if (! expected_grp) {
  std::cerr << "*** cannot load group: "
            << system.render(expected_grp.error()) << std::endl;
  return;
}
auto grp = std::move(*expected_grp);
scoped_actor self{system};
self->join(grp);
self->send(grp, "test");
self->receive(
  [](const std::string& str) {
    assert(str == "test");
  }
);
self->leave(grp);
\end{lstlisting}

It is worth mentioning that the module \lstinline`"local"` is guaranteed to
never return an error. The example above uses the general API for retrieving
the group. However, local modules can be easier accessed by calling
\lstinline`system.groups().get_local(id)`, which returns \lstinline`group`
instead of \lstinline`expected<group>`.

\subsection{Anonymous Groups}
\label{anonymous-group}

Groups created on-the-fly with \lstinline^system.groups().anonymous()^ can be
used to coordinate a set of workers. Each call to this function returns a new,
unique group instance.

\subsection{Local Groups}
\label{local-group}

The \lstinline^"local"^ group module creates groups for in-process
communication. For example, a group for GUI related events could be identified
by \lstinline^system.groups().get_local("GUI events")^. The group ID
\lstinline^"GUI events"^ uniquely identifies a singleton group instance of the
module \lstinline^"local"^.

\subsection{Remote Groups}
\label{remote-group}

Calling\lstinline^system.middleman().publish_local_groups(port, addr)^ makes
all local groups available to other nodes in the network. The first argument
denotes the port, while the second (optional) parameter can be used to
whitelist IP addresses.

After publishing the group at one node (the server), other nodes (the clients)
can get a handle for that group by using the ``remote'' module:
\lstinline^system.groups().get("remote", "<group>@<host>:<port>")^. This
implementation uses N-times unicast underneath and the group is only available
as long as the hosting server is alive.
