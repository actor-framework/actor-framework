\section{Frequently Asked Questions}
\label{faq}

This Section is a compilation of the most common questions via GitHub, chat,
and mailing list.

\subsection{Can I Encrypt CAF Communication?}

Yes, by using the OpenSSL module~\see{free-remoting-functions}.

\subsection{Can I Create Messages Dynamically?}

Yes.

Usually, messages are created implicitly when sending messages but can also be
created explicitly using \lstinline^make_message^. In both cases, types and
number of elements are known at compile time. To allow for fully dynamic
message generation, CAF also offers \lstinline^message_builder^:

\begin{lstlisting}
message_builder mb;
// prefix message with some atom
mb.append(strings_atom::value);
// fill message with some strings
std::vector<std::string> strings{/*...*/};
for (auto& str : strings)
  mb.append(str);
// create the message
message msg = mb.to_message();
\end{lstlisting}

\subsection{What Debugging Tools Exist?}

The \lstinline^scripts/^ and \lstinline^tools/^ directories contain some useful
tools to aid in development and debugging.

\lstinline^scripts/atom.py^ converts integer atom values back into strings.

\lstinline^scripts/demystify.py^ replaces cryptic \lstinline^typed_mpi<...>^
templates with \lstinline^replies_to<...>::with<...>^ and
\lstinline^atom_constant<...>^ with a human-readable representation of the
actual atom.

\lstinline^scripts/caf-prof^ is an R script that generates plots from CAF
profiler output.

\lstinline^caf-vec^ is a (highly) experimental tool that annotates CAF logs
with vector timestamps. It gives you happens-before relations and a nice
visualization via \href{https://bestchai.bitbucket.io/shiviz/}{ShiViz}.
