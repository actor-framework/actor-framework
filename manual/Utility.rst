\section{Utility}
\label{utility}

CAF includes a few utility classes that are likely to be part of C++
eventually (or already are in newer versions of the standard). However, until
these classes are part of the standard library on all supported compilers, we
unfortunately have to maintain our own implementations.

\subsection{Class \lstinline^optional^}
\label{optional}

Represents a value that may or may not exist.

\begin{center}
\begin{tabular}{ll}
  \textbf{Constructors} & ~ \\
  \hline
  \lstinline^(T value)^ & Constructs an object with a value. \\
  \hline
  \lstinline^(none_t = none)^ & Constructs an object without a value. \\
  \hline
  ~ & ~ \\ \textbf{Observers} & ~ \\
  \hline
  \lstinline^explicit operator bool()^ & Checks whether the object contains a value. \\
  \hline
  \lstinline^T* operator->()^ & Accesses the contained value. \\
  \hline
  \lstinline^T& operator*()^ & Accesses the contained value. \\
  \hline
\end{tabular}
\end{center}

\subsection{Class \lstinline^expected^}

Represents the result of a computation that \emph{should} return a value. If no
value could be produced, the \lstinline^expected<T>^ contains an
\lstinline^error^ \see{error}.

\begin{center}
\begin{tabular}{ll}
  \textbf{Constructors} & ~ \\
  \hline
  \lstinline^(T value)^ & Constructs an object with a value. \\
  \hline
  \lstinline^(error err)^ & Constructs an object with an error. \\
  \hline
  ~ & ~ \\ \textbf{Observers} & ~ \\
  \hline
  \lstinline^explicit operator bool()^ & Checks whether the object contains a value. \\
  \hline
  \lstinline^T* operator->()^ & Accesses the contained value. \\
  \hline
  \lstinline^T& operator*()^ & Accesses the contained value. \\
  \hline
  \lstinline^error& error()^ & Accesses the contained error. \\
  \hline
\end{tabular}
\end{center}


\subsection{Constant \lstinline^unit^}

The constant \lstinline^unit^ of type \lstinline^unit_t^ is the equivalent of
\lstinline^void^ and can be used to initialize \lstinline^optional<void>^ and
\lstinline^expected<void>^.

\subsection{Constant \lstinline^none^}

The constant \lstinline^none^ of type \lstinline^none_t^ can be used to
initialize an \lstinline^optional<T>^ to represent ``nothing''.
