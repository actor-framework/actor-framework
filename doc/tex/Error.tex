\section{Errors}
\label{error}

Errors in CAF have a code and a category, similar to
\lstinline^std::error_code^ and \lstinline^std::error_condition^. Unlike its
counterparts from the C++ standard library, \lstinline^error^ is
plattform-neutral and serializable. Instead of using category singletons, CAF
stores categories as atoms~\see{atom}. Errors can also include a message to
provide additional context information.

\subsection{Class Interface}

\begin{center}
\begin{tabular}{ll}
  \textbf{Constructors} & ~ \\
  \hline
  \lstinline^(Enum x)^ & Construct error by calling \lstinline^make_error(x)^ \\
  \hline
  \lstinline^(uint8_t x, atom_value y)^ & Construct error with code \lstinline^x^ and category \lstinline^y^ \\
  \hline
  \lstinline^(uint8_t x, atom_value y, message z)^ & Construct error with code \lstinline^x^, category \lstinline^y^, and context \lstinline^z^ \\
  \hline
  ~ & ~ \\ \textbf{Observers} & ~ \\
  \hline
  \lstinline^uint8_t code()^ & Returns the error code \\
  \hline
  \lstinline^atom_value category()^ & Returns the error category \\
  \hline
  \lstinline^message context()^ & Returns additional context information \\
  \hline
  \lstinline^explicit operator bool()^ & Returns \lstinline^code() != 0^ \\
  \hline
\end{tabular}
\end{center}

\subsection{Add Custom Error Categories}
\label{custom-error}

Adding custom error categories requires three steps: (1)~declare an enum class
of type \lstinline^uint8_t^ with the first value starting at 1, (2)~implement a
free function \lstinline^make_error^ that converts the enum to an
\lstinline^error^ object, (3)~add the custom category to the actor system with
a render function. The last step is optional to allow users to retrieve a
better string representation from \lstinline^system.render(x)^ than
\lstinline^to_string(x)^ can offer. Note that any error code with value 0 is
interpreted as \emph{not-an-error}. The following example adds a custom error
category by performing the first two steps.

\cppexample[19-34]{message_passing/divider}

The implementation of \lstinline^to_string(error)^ is unable to call string
conversions for custom error categories. Hence,
\lstinline^to_string(make_error(math_error::division_by_zero))^ returns
\lstinline^"error(1, math)"^.

The following code adds a rendering function to the actor system to provide a
more satisfactory string conversion.

\cppexample[50-58]{message_passing/divider}

With the custom rendering function,
\lstinline^system.render(make_error(math_error::division_by_zero))^ returns
\lstinline^"math_error(division_by_zero)"^.

\clearpage
\subsection{System Error Codes}
\label{sec}

System Error Codes (SECs) use the error category \lstinline^"system"^. They
represent errors in the actor system or one of its modules and are defined as
follows.

\sourcefile[32-117]{libcaf_core/caf/sec.hpp}

%\clearpage
\subsection{Default Exit Reasons}
\label{exit-reason}

CAF uses the error category \lstinline^"exit"^ for default exit reasons. These
errors are usually fail states set by the actor system itself. The two
exceptions are \lstinline^exit_reason::user_shutdown^ and
\lstinline^exit_reason::kill^. The former is used in CAF to signalize orderly,
user-requested shutdown and can be used by programmers in the same way. The
latter terminates an actor unconditionally when used in \lstinline^send_exit^,
even if the default handler for exit messages~\see{exit-message} is overridden.

\sourcefile[29-49]{libcaf_core/caf/exit_reason.hpp}
