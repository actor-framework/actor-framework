\section{Registry}
\label{registry}

The actor registry in CAF keeps track of the number of running actors and
allows to map actors to their ID or a custom atom~\see{atom} representing a
name. The registry does \emph{not} contain all actors. Actors have to be stored
in the registry explicitly. Users can access the registry through an actor
system by calling \lstinline^system.registry()^. The registry stores actors
using \lstinline^strong_actor_ptr^ \see{actor-pointer}.

Users can use the registry to make actors system-wide available by name. The
middleman~\see{middleman} uses the registry to keep track of all actors known
to remote nodes in order to serialize and deserialize them. Actors are removed
automatically when they terminate.

It is worth mentioning that the registry is not synchronized between connected
actor system. Each actor system has its own, local registry in a distributed
setting.

\begin{center}
\begin{tabular}{ll}
  \textbf{Types} & ~ \\
  \hline
  \lstinline^name_map^ & \lstinline^unordered_map<atom_value, strong_actor_ptr>^ \\
  \hline
  ~ & ~ \\ \textbf{Observers} & ~ \\
  \hline
  \lstinline^strong_actor_ptr get(actor_id)^ & Returns the actor associated to given ID. \\
  \hline
  \lstinline^strong_actor_ptr get(atom_value)^ & Returns the actor associated to given name. \\
  \hline
  \lstinline^name_map named_actors()^ & Returns all name mappings. \\
  \hline
  \lstinline^size_t running()^ & Returns the number of currently running actors. \\
  \hline
  ~ & ~ \\ \textbf{Modifiers} & ~ \\
  \hline
  \lstinline^void put(actor_id, strong_actor_ptr)^ & Maps an actor to its ID. \\
  \hline
  \lstinline^void erase(actor_id)^ & Removes an ID mapping from the registry. \\
  \hline
  \lstinline^void put(atom_value, strong_actor_ptr)^ & Maps an actor to a name. \\
  \hline
  \lstinline^void erase(atom_value)^ & Removes a name mapping from the registry. \\
  \hline
\end{tabular}
\end{center}
