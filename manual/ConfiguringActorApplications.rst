\section{Configuring Actor Applications}
\label{system-config}

CAF configures applications at startup using an
\lstinline^actor_system_config^ or a user-defined subclass of that type. The
config objects allow users to add custom types, to load modules, and to
fine-tune the behavior of loaded modules with command line options or
configuration files~\see{system-config-options}.

The following code example is a minimal CAF application with a
middleman~\see{middleman} but without any custom configuration options.

\begin{lstlisting}
void caf_main(actor_system& system) {
  // ...
}
CAF_MAIN(io::middleman)
\end{lstlisting}

The compiler expands this example code to the following.

\begin{lstlisting}
void caf_main(actor_system& system) {
  // ...
}
int main(int argc, char** argv) {
  return exec_main<io::middleman>(caf_main, argc, argv);
}
\end{lstlisting}

The function \lstinline^exec_main^ creates a config object, loads all modules
requested in \lstinline^CAF_MAIN^ and then calls \lstinline^caf_main^. A
minimal implementation for \lstinline^main^ performing all these steps manually
is shown in the next example for the sake of completeness.

\begin{lstlisting}
int main(int argc, char** argv) {
  actor_system_config cfg;
  // read CLI options
  cfg.parse(argc, argv);
  // return immediately if a help text was printed
  if (cfg.cli_helptext_printed)
    return 0;
  // load modules
  cfg.load<io::middleman>();
  // create actor system and call caf_main
  actor_system system{cfg};
  caf_main(system);
}
\end{lstlisting}

However, setting up config objects by hand is usually not necessary. CAF
automatically selects user-defined subclasses of
\lstinline^actor_system_config^ if \lstinline^caf_main^ takes a second
parameter by reference, as shown in the minimal example below.

\begin{lstlisting}
class my_config : public actor_system_config {
public:
  my_config() {
    // ...
  }
};

void caf_main(actor_system& system, const my_config& cfg) {
  // ...
}

CAF_MAIN()
\end{lstlisting}

Users can perform additional initialization, add custom program options, etc.
simply by implementing a default constructor.

\subsection{Loading Modules}
\label{system-config-module}

The simplest way to load modules is to use the macro \lstinline^CAF_MAIN^ and
to pass a list of all requested modules, as shown below.

\begin{lstlisting}
void caf_main(actor_system& system) {
  // ...
}
CAF_MAIN(mod1, mod2, ...)
\end{lstlisting}

Alternatively, users can load modules in user-defined config classes.

\begin{lstlisting}
class my_config : public actor_system_config {
public:
  my_config() {
    load<mod1>();
    load<mod2>();
    // ...
  }
};
\end{lstlisting}

The third option is to simply call \lstinline^x.load<mod1>()^ on a config
object \emph{before} initializing an actor system with it.

\subsection{Command Line Options and INI Configuration Files}
\label{system-config-options}

CAF organizes program options in categories and parses CLI arguments as well
as INI files. CLI arguments override values in the INI file which override
hard-coded defaults. Users can add any number of custom program options by
implementing a subtype of \lstinline^actor_system_config^. The example below
adds three options to the ``global'' category.

\cppexample[222-234]{remoting/distributed_calculator}

We create a new ``global'' category in  \lstinline^custom_options_}^. Each
following call to \lstinline^add^ then appends individual options to the
category. The first argument to \lstinline^add^ is the associated variable. The
second argument is the name for the parameter, optionally suffixed with a
comma-separated single-character short name. The short name is only considered
for CLI parsing and allows users to abbreviate commonly used option names. The
third and final argument to \lstinline^add^ is a help text.

The custom \lstinline^config^ class allows end users to set the port for the
application to 42 with either \lstinline^--port=42^ (long name) or
\lstinline^-p 42^ (short name). The long option name is prefixed by the
category when using a different category than ``global''. For example, adding
the port option to the category ``foo'' means end users have to type
\lstinline^--foo.port=42^ when using the long name. Short names are unaffected
by the category, but have to be unique.

Boolean options do not require arguments. The member variable
\lstinline^server_mode^ is set to \lstinline^true^ if the command line contains
either \lstinline^--server-mode^ or \lstinline^-s^.

The example uses member variables for capturing user-provided settings for
simplicity. However, this is not required. For example,
\lstinline^add<bool>(...)^ allows omitting the first argument entirely. All
values of the configuration are accessible with \lstinline^get_or^. Note that
all global options can omit the \lstinline^"global."^ prefix.

CAF adds the program options ``help'' (with short names \lstinline^-h^ and
\lstinline^-?^) as well as ``long-help'' to the ``global'' category.

The default name for the INI file is \lstinline^caf-application.ini^. Users can
change the file name and path by passing \lstinline^--config-file=<path>^ on
the command line.

INI files are organized in categories. No value is allowed outside of a
category (no implicit ``global'' category). The parses uses the following
syntax:

\begin{tabular}{p{0.3\textwidth}p{0.65\textwidth}}
  \lstinline^key=true^ & is a boolean \\
  \lstinline^key=1^ & is an integer \\
  \lstinline^key=1.0^ & is an floating point number \\
  \lstinline^key=1ms^ & is an timespan \\
  \lstinline^key='foo'^ & is an atom \\
  \lstinline^key="foo"^ & is a string \\
  \lstinline^key=[0, 1, ...]^ & is as a list \\
  \lstinline^key={a=1, b=2, ...}^ & is a dictionary (map) \\
\end{tabular}

The following example INI file lists all standard options in CAF and their
default value. Note that some options such as \lstinline^scheduler.max-threads^
are usually detected at runtime and thus have no hard-coded default.

\clearpage
\iniexample{caf-application}

\clearpage
\subsection{Adding Custom Message Types}
\label{add-custom-message-type}

CAF requires serialization support for all of its message types
\see{type-inspection}. However, CAF also needs a mapping of unique type names
to user-defined types at runtime. This is required to deserialize arbitrary
messages from the network.

As an introductory example, we (again) use the following POD type
\lstinline^foo^.

\cppexample[24-27]{custom_type/custom_types_1}

To make \lstinline^foo^ serializable, we make it inspectable
\see{type-inspection}:

\cppexample[30-34]{custom_type/custom_types_1}

Finally, we give \lstinline^foo^ a platform-neutral name and add it to the list
of serializable types by using a custom config class.

\cppexample[75-78,81-84]{custom_type/custom_types_1}

\subsection{Adding Custom Error Types}

Adding a custom error type to the system is a convenience feature to allow
improve the string representation. Error types can be added by implementing a
render function and passing it to \lstinline^add_error_category^, as shown
in~\sref{custom-error}.

\clearpage
\subsection{Adding Custom Actor Types \experimental}
\label{add-custom-actor-type}

Adding actor types to the configuration allows users to spawn actors by their
name. In particular, this enables spawning of actors on a different node
\see{remote-spawn}. For our example configuration, we consider the following
simple \lstinline^calculator^ actor.

\cppexample[33-39]{remoting/remote_spawn}

Adding the calculator actor type to our config is achieved by calling
\lstinline^add_actor_type<T>^. Note that adding an actor type in this way
implicitly calls \lstinline^add_message_type<T>^ for typed actors
\see{add-custom-message-type}. This makes our \lstinline^calculator^ actor type
serializable and also enables remote nodes to spawn calculators anywhere in the
distributed actor system (assuming all nodes use the same config).

\cppexample[99-101,106-106,110-101]{remoting/remote_spawn}

Our final example illustrates how to spawn a \lstinline^calculator^ locally by
using its type name. Because the dynamic type name lookup can fail and the
construction arguments passed as message can mismatch, this version of
\lstinline^spawn^ returns \lstinline^expected<T>^.

\begin{lstlisting}
auto x = system.spawn<calculator>("calculator", make_message());
if (! x) {
  std::cerr << "*** unable to spawn calculator: "
            << system.render(x.error()) << std::endl;
  return;
}
calculator c = std::move(*x);
\end{lstlisting}

Adding dynamically typed actors to the config is achieved in the same way. When
spawning a dynamically typed actor in this way, the template parameter is
simply \lstinline^actor^. For example, spawning an actor "foo" which requires
one string is created with:

\begin{lstlisting}
auto worker = system.spawn<actor>("foo", make_message("bar"));
\end{lstlisting}

Because constructor (or function) arguments for spawning the actor are stored
in a \lstinline^message^, only actors with appropriate input types are allowed.
For example, pointer types are illegal. Hence users need to replace C-strings
with \lstinline^std::string^.

\clearpage
\subsection{Log Output}
\label{log-output}

Logging is disabled in CAF per default. It can be enabled by setting the
\lstinline^--with-log-level=^ option of the \lstinline^configure^ script to one
of ``error'', ``warning'', ``info'', ``debug'', or ``trace'' (from least output
to most). Alternatively, setting the CMake variable \lstinline^CAF_LOG_LEVEL^
to 0, 1, 2, 3, or 4 (from least output to most) has the same effect.

All logger-related configuration options listed here and in
\sref{system-config-options} are silently ignored if logging is disabled.

\subsubsection{File Name}
\label{log-output-file-name}

The output file is generated from the template configured by
\lstinline^logger-file-name^. This template supports the following variables.

\begin{tabular}{|p{0.2\textwidth}|p{0.75\textwidth}|}
  \hline
  \textbf{Variable} & \textbf{Output} \\
  \hline
  \texttt{[PID]} & The OS-specific process ID. \\
  \hline
  \texttt{[TIMESTAMP]} & The UNIX timestamp on startup. \\
  \hline
  \texttt{[NODE]} & The node ID of the CAF system. \\
  \hline
\end{tabular}

\subsubsection{Console}
\label{log-output-console}

Console output is disabled per default. Setting \lstinline^logger-console^ to
either \lstinline^"uncolored"^ or \lstinline^"colored"^ prints log events to
\lstinline^std::clog^. Using the \lstinline^"colored"^ option will print the
log events in different colors depending on the severity level.

\subsubsection{Format Strings}
\label{log-output-format-strings}

CAF uses log4j-like format strings for configuring printing of individual
events via \lstinline^logger-file-format^ and
\lstinline^logger-console-format^. Note that format modifiers are not supported
at the moment. The recognized field identifiers are:

\begin{tabular}{|p{0.1\textwidth}|p{0.85\textwidth}|}
  \hline
  \textbf{Character} & \textbf{Output} \\
  \hline
  \texttt{c} & The category/component. This name is defined by the macro \lstinline^CAF_LOG_COMPONENT^. Set this macro before including any CAF header. \\
  \hline
  \texttt{C} & The full qualifier of the current function. For example, the qualifier of \lstinline^void ns::foo::bar()^ is printed as \lstinline^ns.foo^. \\
  \hline
  \texttt{d} & The date in ISO 8601 format, i.e., \lstinline^"YYYY-MM-DD hh:mm:ss"^. \\
  \hline
  \texttt{F} & The file name. \\
  \hline
  \texttt{L} & The line number. \\
  \hline
  \texttt{m} & The user-defined log message. \\
  \hline
  \texttt{M} & The name of the current function. For example, the name of \lstinline^void ns::foo::bar()^ is printed as \lstinline^bar^. \\
  \hline
  \texttt{n} & A newline. \\
  \hline
  \texttt{p} & The priority (severity level). \\
  \hline
  \texttt{r} & Elapsed time since starting the application in milliseconds. \\
  \hline
  \texttt{t} & ID of the current thread. \\
  \hline
  \texttt{a} & ID of the current actor (or ``actor0'' when not logging inside an actor). \\
  \hline
  \texttt{\%} & A single percent sign. \\
  \hline
\end{tabular}

\subsubsection{Filtering}
\label{log-output-filtering}

The two configuration options \lstinline^logger-component-filter^ and
\lstinline^logger-verbosity^ reduce the amount of generated log events. The
former is a list of excluded component names and the latter can increase the
reported severity level (but not decrease it beyond the level defined at
compile time).
