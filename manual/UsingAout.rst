\section{Using \texttt{aout} -- A Concurrency-safe Wrapper for \texttt{cout}}

When using \lstinline^cout^ from multiple actors, output often appears
interleaved. Moreover, using \lstinline^cout^ from multiple actors -- and thus
from multiple threads -- in parallel should be avoided regardless, since the
standard does not guarantee a thread-safe implementation.

By replacing \texttt{std::cout} with \texttt{caf::aout}, actors can achieve a
concurrency-safe text output. The header \lstinline^caf/all.hpp^ also defines
overloads for \texttt{std::endl} and \texttt{std::flush} for \lstinline^aout^,
but does not support the full range of ostream operations (yet). Each write
operation to \texttt{aout} sends a message to a `hidden' actor. This actor only
prints lines, unless output is forced using \lstinline^flush^. The example
below illustrates printing of lines of text from multiple actors (in random
order).

\cppexample{aout}
