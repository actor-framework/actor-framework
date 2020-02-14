\section{Type-Erased Tuples, Messages and Message Views}
\label{message}

Messages in CAF are stored in type-erased tuples. The actual message type
itself is usually hidden, as actors use pattern matching to decompose messages
automatically. However, the classes \lstinline^message^ and
\lstinline^message_builder^ allow more advanced use cases than only sending
data from one actor to another.

The interface \lstinline^type_erased_tuple^ encapsulates access to arbitrary
data. This data can be stored on the heap or on the stack. A
\lstinline^message^ is a type-erased tuple that is always heap-allocated and
uses copy-on-write semantics. When dealing with "plain" type-erased tuples,
users are required to check if a tuple is referenced by others via
\lstinline^type_erased_tuple::shared^ before modifying its content.

The convenience class \lstinline^message_view^ holds a reference to either a
stack-located \lstinline^type_erased_tuple^ or a \lstinline^message^. The
content of the data can be access via \lstinline^message_view::content^ in both
cases, which returns a \lstinline^type_erased_tuple&^. The content of the view
can be forced into a message object by calling
\lstinline^message_view::move_content_to_message^. This member function either
returns the stored message object or moves the content of a stack-allocated
tuple into a new message.

\subsection{RTTI and Type Numbers}

All builtin types in CAF have a non-zero 6-bit \emph{type number}. All
user-defined types are mapped to 0. When querying the run-time type information
(RTTI) for individual message or tuple elements, CAF returns a pair consisting
of an integer and a pointer to \lstinline^std::type_info^. The first value is
the 6-bit type number. If the type number is non-zero, the second value is a
pointer to the C++ type info, otherwise the second value is null. Additionally,
CAF generates 32 bit \emph{type tokens}. These tokens are \emph{type hints}
that summarizes all types in a type-erased tuple. Two type-erased tuples are of
different type if they have different type tokens (the reverse is not true).

\clearpage
\subsection{Class \lstinline^type_erased_tuple^}

\textbf{Note}: Calling modifiers on a shared type-erased tuple is undefined
behavior.

\begin{center}
\begin{tabular}{ll}
  \textbf{Observers} & ~ \\
  \hline
  \lstinline^bool empty()^ & Returns whether this message is empty. \\
  \hline
  \lstinline^size_t size()^ & Returns the size of this message. \\
  \hline
  \lstinline^rtti_pair type(size_t pos)^ & Returns run-time type information for the nth element. \\
  \hline
  \lstinline^error save(serializer& x)^ & Writes the tuple to \lstinline^x^. \\
  \hline
  \lstinline^error save(size_t n, serializer& x)^ & Writes the nth element to \lstinline^x^. \\
  \hline
  \lstinline^const void* get(size_t n)^ & Returns a const pointer to the nth element. \\
  \hline
  \lstinline^std::string stringify()^ & Returns a string representation of the tuple. \\
  \hline
  \lstinline^std::string stringify(size_t n)^ & Returns a string representation of the nth element. \\
  \hline
  \lstinline^bool matches(size_t n, rtti_pair)^ & Checks whether the nth element has given type. \\
  \hline
  \lstinline^bool shared()^ & Checks whether more than one reference to the data exists. \\
  \hline
  \lstinline^bool match_element<T>(size_t n)^ & Checks whether element \lstinline^n^ has type \lstinline^T^. \\
  \hline
  \lstinline^bool match_elements<Ts...>()^ & Checks whether this message has the types \lstinline^Ts...^. \\
  \hline
  \lstinline^const T& get_as<T>(size_t n)^ & Returns a const reference to the nth element. \\
  \hline
  ~ & ~ \\ \textbf{Modifiers} & ~ \\
  \hline
  \lstinline^void* get_mutable(size_t n)^ & Returns a mutable pointer to the nth element. \\
  \hline
  \lstinline^T& get_mutable_as<T>(size_t n)^ & Returns a mutable reference to the nth element. \\
  \hline
  \lstinline^void load(deserializer& x)^ & Reads the tuple from \lstinline^x^. \\
  \hline
\end{tabular}
\end{center}

\subsection{Class \lstinline^message^}

The class \lstinline^message^ includes all member functions of
\lstinline^type_erased_tuple^. However, calling modifiers is always guaranteed
to be safe. A \lstinline^message^ automatically detaches its content by copying
it from the shared data on mutable access. The class further adds the following
member functions over \lstinline^type_erased_tuple^. Note that
\lstinline^apply^ only detaches the content if a callback takes mutable
references as arguments.

\begin{center}
\begin{tabular}{ll}
  \textbf{Observers} & ~ \\
  \hline
  \lstinline^message drop(size_t n)^ & Creates a new message with all but the first \lstinline^n^ values. \\
  \hline
  \lstinline^message drop_right(size_t n)^ & Creates a new message with all but the last \lstinline^n^ values. \\
  \hline
  \lstinline^message take(size_t n)^ & Creates a new message from the first \lstinline^n^ values. \\
  \hline
  \lstinline^message take_right(size_t n)^ & Creates a new message from the last \lstinline^n^ values. \\
  \hline
  \lstinline^message slice(size_t p, size_t n)^ & Creates a new message from \lstinline^[p, p + n)^. \\
  \hline
  \lstinline^message extract(message_handler)^ & See \sref{extract}. \\
  \hline
  \lstinline^message extract_opts(...)^ & See \sref{extract-opts}. \\
  \hline
  ~ & ~ \\ \textbf{Modifiers} & ~ \\
  \hline
  \lstinline^optional<message> apply(message_handler f)^ & Returns \lstinline^f(*this)^. \\
  \hline
  ~ & ~ \\ \textbf{Operators} & ~ \\
  \hline
  \lstinline^message operator+(message x, message y)^ & Concatenates \lstinline^x^ and \lstinline^y^. \\
  \hline
  \lstinline^message& operator+=(message& x, message y)^ & Concatenates \lstinline^x^ and \lstinline^y^. \\
  \hline
\end{tabular}
\end{center}

\clearpage
\subsection{Class \texttt{message\_builder}}

\begin{center}
\begin{tabular}{ll}
  \textbf{Constructors} & ~ \\
  \hline
  \lstinline^(void)^ & Creates an empty message builder. \\
  \hline
  \lstinline^(Iter first, Iter last)^ & Adds all elements from range \lstinline^[first, last)^. \\
  \hline
  ~ & ~ \\ \textbf{Observers} & ~ \\
  \hline
  \lstinline^bool empty()^ & Returns whether this message is empty. \\
  \hline
  \lstinline^size_t size()^ & Returns the size of this message. \\
  \hline
  \lstinline^message to_message(	)^ & Converts the buffer to an actual message object. \\
  \hline
  \lstinline^append(T val)^ & Adds \lstinline^val^ to the buffer. \\
  \hline
  \lstinline^append(Iter first, Iter last)^ & Adds all elements from range \lstinline^[first, last)^. \\
  \hline
  \lstinline^message extract(message_handler)^ & See \sref{extract}. \\
  \hline
  \lstinline^message extract_opts(...)^ & See \sref{extract-opts}. \\
  \hline
  ~ & ~ \\ \textbf{Modifiers} & ~ \\
  \hline
  \lstinline^optional<message>^ \lstinline^apply(message_handler f)^ & Returns \lstinline^f(*this)^. \\
  \hline
  \lstinline^message move_to_message()^ & Transfers ownership of its data to the new message. \\
  \hline
\end{tabular}
\end{center}

\clearpage
\subsection{Extracting}
\label{extract}

The member function \lstinline^message::extract^ removes matched elements from
a message. x Messages are filtered by repeatedly applying a message handler to
the greatest remaining slice, whereas slices are generated in the sequence $[0,
size)$, $[0, size-1)$, $...$, $[1, size-1)$, $...$, $[size-1, size)$. Whenever
a slice is matched, it is removed from the message and the next slice starts at
the same index on the reduced message.

For example:

\begin{lstlisting}
auto msg = make_message(1, 2.f, 3.f, 4);
// remove float and integer pairs
auto msg2 = msg.extract({
  [](float, float) { },
  [](int, int) { }
});
assert(msg2 == make_message(1, 4));
\end{lstlisting}

Step-by-step explanation:

\begin{itemize}
  \item Slice 1: \lstinline^(1, 2.f, 3.f, 4)^, no match
  \item Slice 2: \lstinline^(1, 2.f, 3.f)^, no match
  \item Slice 3: \lstinline^(1, 2.f)^, no match
  \item Slice 4: \lstinline^(1)^, no match
  \item Slice 5: \lstinline^(2.f, 3.f, 4)^, no match
  \item Slice 6: \lstinline^(2.f, 3.f)^, \emph{match}; new message is \lstinline^(1, 4)^
  \item Slice 7: \lstinline^(4)^, no match
\end{itemize}

Slice 7 is \lstinline^(4)^, i.e., does not contain the first element, because
the match on slice 6 occurred at index position 1. The function
\lstinline^extract^ iterates a message only once, from left to right. The
returned message contains the remaining, i.e., unmatched, elements.

\clearpage
\subsection{Extracting Command Line Options}
\label{extract-opts}

The class \lstinline^message^ also contains a convenience interface to
\lstinline^extract^ for parsing command line options: the member function
\lstinline^extract_opts^.

\begin{lstlisting}
int main(int argc, char** argv) {
  uint16_t port;
  string host = "localhost";
  auto res = message_builder(argv + 1, argv + argc).extract_opts({
    {"port,p", "set port", port},
    {"host,H", "set host (default: localhost)", host},
    {"verbose,v", "enable verbose mode"}
  });
  if (! res.error.empty()) {
    // read invalid CLI arguments
    cerr << res.error << endl;
    return 1;
  }
  if (res.opts.count("help") > 0) {
    // CLI arguments contained "-h", "--help", or "-?" (builtin);
    cout << res.helptext << endl;
    return 0;
  }
  if (! res.remainder.empty()) {
    // res.remainder stors all extra arguments that weren't consumed
  }
  if (res.opts.count("verbose") > 0) {
    // enable verbose mode
  }
  // ...
}

/*
Output of ./program_name -h:

Allowed options:
  -p [--port] arg  : set port
  -H [--host] arg  : set host (default: localhost)
  -v [--verbose]   : enable verbose mode
*/
\end{lstlisting}
