\section{Type Inspection (Serialization and String Conversion)}
\label{type-inspection}

CAF is designed with distributed systems in mind. Hence, all message types
must be serializable and need a platform-neutral, unique name that is
configured at startup \see{add-custom-message-type}. Using a message type that
is not serializable causes a compiler error \see{unsafe-message-type}. CAF
serializes individual elements of a message by using the inspection API. This
API allows users to provide code for serialization as well as string conversion
with a single free function. The signature for a class \lstinline^my_class^ is
always as follows:

\begin{lstlisting}
template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, my_class& x) {
  return f(...);
}
\end{lstlisting}

The function \lstinline^inspect^ passes meta information and data fields to the
variadic call operator of the inspector. The following example illustrates an
implementation for \lstinline^inspect^ for a simple POD struct.

\cppexample[23-33]{custom_type/custom_types_1}

The inspector recursively inspects all data fields and has builtin support for
(1) \lstinline^std::tuple^, (2) \lstinline^std::pair^, (3) C arrays, (4) any
container type with \lstinline^x.size()^, \lstinline^x.empty()^,
\lstinline^x.begin()^ and \lstinline^x.end()^.

We consciously made the inspect API as generic as possible to allow for
extensibility. This allows users to use CAF's types in other contexts, to
implement parsers, etc.

\subsection{Inspector Concept}

The following concept class shows the requirements for inspectors. The
placeholder \lstinline^T^ represents any user-defined type. For example,
\lstinline^error^ when performing I/O operations or some integer type when
implementing a hash function.

\begin{lstlisting}
Inspector {
  using result_type = T;

  if (inspector only requires read access to the state of T)
    static constexpr bool reads_state = true;
  else
    static constexpr bool writes_state = true;

  template <class... Ts>
  result_type operator()(Ts&&...);
}
\end{lstlisting}

A saving \lstinline^Inspector^ is required to handle constant lvalue and rvalue
references. A loading \lstinline^Inspector^ must only accept mutable lvalue
references to data fields, but still allow for constant lvalue references and
rvalue references to annotations.

\subsection{Annotations}

Annotations allow users to fine-tune the behavior of inspectors by providing
addition meta information about a type. All annotations live in the namespace
\lstinline^caf::meta^ and derive from \lstinline^caf::meta::annotation^. An
inspector can query whether a type \lstinline^T^ is an annotation with
\lstinline^caf::meta::is_annotation<T>::value^. Annotations are passed to the
call operator of the inspector along with data fields. The following list shows
all annotations supported by CAF:

\begin{itemize}
\item \lstinline^type_name(n)^: Display type name as \lstinline^n^ in
  human-friendly output (position before data fields).
\item \lstinline^hex_formatted()^: Format the following data field in hex
  format.
\item \lstinline^omittable()^: Omit the following data field in human-friendly
  output.
\item \lstinline^omittable_if_empty()^: Omit the following data field if it is
  empty in human-friendly output.
\item \lstinline^omittable_if_none()^: Omit the following data field if it
  equals \lstinline^none^ in human-friendly output.
\item \lstinline^save_callback(f)^: Call \lstinline^f^ when serializing
  (position after data fields).
\item \lstinline^load_callback(f)^: Call \lstinline^f^ after deserializing all
  data fields (position after data fields).
\end{itemize}

\subsection{Backwards and Third-party Compatibility}

CAF evaluates common free function other than \lstinline^inspect^ in order to
simplify users to integrate CAF into existing code bases.

Serializers and deserializers call user-defined \lstinline^serialize^
functions. Both types support \lstinline^operator&^ as well as
\lstinline^operator()^ for individual data fields. A \lstinline^serialize^
function has priority over \lstinline^inspect^.

When converting a user-defined type to a string, CAF calls user-defined
\lstinline^to_string^ functions and prefers those over \lstinline^inspect^.

\subsection{Whitelisting Unsafe Message Types}
\label{unsafe-message-type}

Message types that are not serializable cause compile time errors when used in
actor communication. When using CAF for concurrency only, this errors can be
suppressed by whitelisting types with
\lstinline^CAF_ALLOW_UNSAFE_MESSAGE_TYPE^. The macro is defined as follows.

% TODO: expand example here\cppexample[linerange={50-54}]{../../libcaf_core/caf/allowed_unsafe_message_type.hpp}

\clearpage
\subsection{Splitting Save and Load Operations}

If loading and storing cannot be implemented in a single function, users can
query whether the inspector is loading or storing. For example, consider the
following class \lstinline^foo^ with getter and setter functions and no public
access to its members.

\cppexample[20-49]{custom_type/custom_types_3}

\clearpage
Since there is no access to the data fields \lstinline^a_^ and \lstinline^b_^
(and assuming no changes to \lstinline^foo^ are possible), we need to split our
implementation of \lstinline^inspect^ as shown below.

\cppexample[76-103]{custom_type/custom_types_3}

The purpose of the scope guard in the example above is to write the content of
the temporaries back to \lstinline^foo^ at scope exit automatically. Storing
the result of \lstinline^f(...)^ in a temporary first and then writing the
changes to \lstinline^foo^ is not possible, because \lstinline^f(...)^ can
return \lstinline^void^.
