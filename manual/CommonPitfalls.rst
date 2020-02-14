\section{Common Pitfalls}
\label{pitfalls}

This Section highlights common mistakes or C++ subtleties that can show up when
programming in CAF.

\subsection{Defining Message Handlers}

C++ evaluates comma-separated expressions from left-to-right, using only the
last element as return type of the whole expression. This means that message
handlers and behaviors must \emph{not} be initialized like this:

\begin{lstlisting}
message_handler wrong = (
  [](int i) { /*...*/ },
  [](float f) { /*...*/ }
);
\end{lstlisting}

The correct way to initialize message handlers and behaviors is to either
use the constructor or the member function \lstinline^assign^:

\begin{lstlisting}
message_handler ok1{
  [](int i) { /*...*/ },
  [](float f) { /*...*/ }
};

message_handler ok2;
// some place later
ok2.assign(
  [](int i) { /*...*/ },
  [](float f) { /*...*/ }
);
\end{lstlisting}

\subsection{Event-Based API}

The member function \lstinline^become^ does not block, i.e., always returns
immediately. Thus, lambda expressions should \textit{always} capture by value.
Otherwise, all references on the stack will cause undefined behavior if the
lambda expression is executed.

\subsection{Requests}

A handle returned by \lstinline^request^ represents \emph{exactly one} response
message. It is not possible to receive more than one response message.

The handle returned by \lstinline^request^ is bound to the calling actor. It is
not possible to transfer a handle to a response to another actor.

\clearpage
\subsection{Sharing}

It is strongly recommended to \textbf{not} share states between actors. In
particular, no actor shall ever access member variables or member functions of
another actor. Accessing shared memory segments concurrently can cause undefined
behavior that is incredibly hard to find and debug. However, sharing
\textit{data} between actors is fine, as long as the data is \textit{immutable}
and its lifetime is guaranteed to outlive all actors. The simplest way to meet
the lifetime guarantee is by storing the data in smart pointers such as
\lstinline^std::shared_ptr^. Nevertheless, the recommended way of sharing
informations is message passing. Sending the same message to multiple actors
does not result in copying the data several times.
