.. _system-config:

Configuring Actor Applications
==============================

CAF configures applications at startup using an
``actor_system_config`` or a user-defined subclass of that type. The
config objects allow users to add custom types, to load modules, and to
fine-tune the behavior of loaded modules with command line options or
configuration files system-config-options_.

The following code example is a minimal CAF application with a :ref:`middleman`
but without any custom configuration options.

.. code-block:: C++

   void caf_main(actor_system& system) {
     // ...
   }
   CAF_MAIN(io::middleman)

The compiler expands this example code to the following.

.. code-block:: C++

   void caf_main(actor_system& system) {
     // ...
   }
   int main(int argc, char** argv) {
     return exec_main<io::middleman>(caf_main, argc, argv);
   }

The function ``exec_main`` performs several steps:

#. Initialize all meta objects for the type ID blocks listed in ``CAF_MAIN``.
#. Create a config object. If ``caf_main`` has two arguments, then CAF
   assumes that the second argument is the configuration and the type gets
   derived from that argument. Otherwise, CAF uses ``actor_system_config``.
#. Parse command line arguments and configuration file.
#. Load all modules requested in ``CAF_MAIN``.
#. Create an actor system.
#. Call ``caf_main`` with the actor system and optionally with ``config``.

When implementing the steps performed by ``CAF_MAIN`` by hand, the ``main``
function would resemble the following (pseudo) code:

.. code-block:: C++

  int main(int argc, char** argv) {
    // Initialize user defined types and messages if needed.
    init_global_meta_objects<custom_types_1>();
    // Initialize the global type information before creating the config.
    core::init_global_meta_objects();
    // Create the config.
    actor_system_config cfg;
    // Read CLI options.
    auto err = cfg.parse(argc, argv);
    if (err)
      return EXIT_FAILURE;
    // Return immediately if a help text was printed.
    if (cfg.helptext_printed())
      return 0;
    // Load modules (the provided example will load `net::middleman`).
    net::middleman::init_global_meta_objects();
    cfg.load<net::middleman>();
    // Create the actor system.
    actor_system sys{cfg};
    // Run user-defined code.
    caf_main(sys, cfg);
  }

Using ``CAF_MAIN`` simply automates that boilerplate code. A minimal example
with a custom type ID block as well as a custom configuration class with the I/O
module loaded looks as follows:

.. code-block:: C++

  CAF_BEGIN_TYPE_ID_BLOCK(my, first_custom_type_id)

    // ...

  CAF_END_TYPE_ID_BLOCK(my)


  class my_config : public actor_system_config {
  public:
    my_config() {
      // ...
    }
  };

  void caf_main(actor_system& system, const my_config& cfg) {
    // ...
  }

  CAF_MAIN(id_block::my, io::middleman)

.. _system-config-module:

Available Modules
-----------------

The core functionalities of CAF can be extended by loading different modules at
runtime. This modular design enables users to fine-tune which features of CAF
to include in their applications, ensuring they don't introduce overhead for
unused functionality. Here is a comprehensive list of available modules:

.. list-table:: CAF Modules
   :widths: 25 75
   :header-rows: 1

   * - Module
     - Description
   * - ``openssl::manager``
     - Adds SSL/TLS encryption support to network communications. Enables
       secure communication between actors over untrusted networks. Requires
       OpenSSL libraries.
   * - ``io::middleman``
     - Provides I/O and socket support, and enables communication between actor
       applications by managing proxy actor instances representing remote
       actors.
   * - ``net::middleman``
     - Provides modern network interfaces with improved type safety and
       configuration options. Designed as the successor to the traditional I/O
       middleman.

The exact modules available may vary depending on the way CAF was configured
and compiled. Every module can be disabled at build time.

.. note::

  Note that the ``io::middleman`` module is in maintenance mode and will be
  deprecated in favor of the ``net::middleman`` module in a future instance
  of CAF.


Loading Modules
---------------

The simplest way to load modules is to use the macro ``CAF_MAIN`` and
to pass a list of all requested modules, as shown below.

.. code-block:: C++

   void caf_main(actor_system& system) {
     // ...
   }
   CAF_MAIN(mod1, mod2, ...)

Alternatively, users can load modules in user-defined config classes.

.. code-block:: C++

   class my_config : public actor_system_config {
   public:
     my_config() {
       load<mod1>();
       load<mod2>();
       // ...
     }
   };

The third option is to simply call ``cfg.load<mod1>()`` on a config
object *before* initializing an actor system with it.

.. _system-config-options:

Program Options
---------------

CAF organizes program options in categories and parses CLI arguments as well as
configuration files. CLI arguments override values in the configuration file
which override hard-coded defaults. Users can add any number of custom program
options by implementing a subtype of ``actor_system_config``. The example below
adds three options to the ``global`` category.

.. literalinclude:: /examples/remoting/distributed_calculator.cpp
   :language: C++
   :start-after: --(rst-config-begin)--
   :end-before: --(rst-config-end)--

We create a new ``global`` category in ``custom_options_``. Each following call
to ``add`` then appends individual options to the category. The first argument
to ``add`` is the associated variable. The second argument is the name for the
parameter, optionally suffixed with a comma-separated single-character short
name. The short name is only considered for CLI parsing and allows users to
abbreviate commonly used option names. The third and final argument to ``add``
is a help text.

The custom ``config`` class allows end users to set the port for the application
to 42 with either ``-p 42`` (short name) or ``--port=42`` (long name). The long
option name is prefixed by the category when using a different category than
``global``. For example, adding the port option to the category ``foo`` means
end users have to type ``--foo.port=42`` when using the long name. Short names
are unaffected by the category, but have to be unique.

Boolean options do not require arguments. The member variable ``server_mode`` is
set to ``true`` if the command line contains either ``--server-mode`` or ``-s``.

The example uses member variables for capturing user-provided settings for
simplicity. However, this is not required. For example, ``add<bool>(...)``
allows omitting the first argument entirely. All values of the configuration are
accessible with ``get_or``. Note that all global options can omit the
``"global."`` prefix.

CAF adds the program options ``help`` (with short names ``-h`` and ``-?``) as
well as ``long-help`` to the ``global`` category.

Configuration Files
-------------------

The default name for the configuration file is ``caf-application.conf``. Users
can change the file path by passing ``--config-file=<path>`` on the command
line.

The syntax for the configuration files provides a clean JSON-like grammar that
is similar to other commonly used configuration formats. In a nutshell, instead
of writing:

.. code-block:: JSON

  {
    "my-category" : {
      "first" : 1,
      "second" : 2
    }
  }

you can reduce the noise by writing:

.. code-block:: none

  my-category {
    first = 1
    second = 2
  }

.. note::

  CAF will accept both of the examples above and will produce the same result.
  We recommend using the second style, mostly because it reduces syntax noise.

Unlike regular JSON, CAF's configuration format supports a couple of additional
syntax elements such as comments (comments start with ``#`` and end at the end
of the line) and, most notably, does *not* accept ``null``.

The parses uses the following syntax for writing key-value pairs:

+-------------------------+-----------------------------+
| ``key=true``            | is a boolean                |
+-------------------------+-----------------------------+
| ``key=1``               | is an integer               |
+-------------------------+-----------------------------+
| ``key=1.0``             | is an floating point number |
+-------------------------+-----------------------------+
| ``key=1ms``             | is an timespan              |
+-------------------------+-----------------------------+
| ``key='foo'``           | is a string                 |
+-------------------------+-----------------------------+
| ``key="foo"``           | is a string                 |
+-------------------------+-----------------------------+
| ``key=[0, 1, ...]``     | is as a list                |
+-------------------------+-----------------------------+
| ``key={a=1, b=2, ...}`` | is a dictionary (map)       |
+-------------------------+-----------------------------+

The following example configuration file lists all standard options in CAF and
their default value. Note that some options such as ``scheduler.max-threads``
are usually detected at runtime and thus have no hard-coded default.

.. literalinclude:: /examples/caf-application.conf
  :language: none

.. _custom-message-types:

Adding Custom Message Types
---------------------------

CAF requires serialization support for all of its message types (see
:ref:`type-inspection`). However, CAF also needs a mapping of unique type IDs to
user-defined types at runtime. This is required to deserialize arbitrary
messages from the network.

The type IDs are assigned by listing all custom types in a *type ID block*. CAF
assigns ascending IDs to each type by in the block as well as storing the type
name. In the following example, we forward-declare the types ``foo`` and
``foo2`` and register them to CAF in a *type ID block*. The name of the type ID
block is arbitrary, but it must be a valid C++ identifier.

.. literalinclude:: /examples/custom_type/custom_types_1.cpp
   :language: C++
   :start-after: --(rst-type-id-block-begin)--
   :end-before: --(rst-type-id-block-end)--

Aside from a type ID, CAF also requires an ``inspect`` overload in order to be
able to serialize objects. As an introductory example, we (again) use the
following POD type ``foo``.

.. literalinclude:: /examples/custom_type/custom_types_1.cpp
   :language: C++
   :start-after: --(rst-foo-begin)--
   :end-before: --(rst-foo-end)--

By assigning type IDs and providing ``inspect`` overloads, we provide static and
compile-time information for all our types. However, CAF also needs some
information at run-time for deserializing received data. The function
``init_global_meta_objects`` takes care of registering all the state we need at
run-time. This function usually gets called by ``CAF_MAIN`` automatically. When
not using this macro, users **must** call ``init_global_meta_objects`` before
any other CAF function.

Adding Custom Error Types
-------------------------

Adding a custom error type to the system is a convenience feature to allow
improve the string representation. Error types can be added by implementing a
render function and passing it to ``add_error_category``, as shown in
:ref:`custom-error`.

.. _add-custom-actor-type:

Adding Custom Actor Types  :sup:`experimental`
----------------------------------------------

Adding actor types to the configuration allows users to spawn actors by their
name. In particular, this enables spawning of actors on a different node (see
:ref:`remote-spawn`). For our example configuration, we consider the following
simple ``calculator`` actor.

.. literalinclude:: /examples/remoting/remote_spawn.cpp
   :language: C++
   :start-after: --(rst-calculator-begin)--
   :end-before: --(rst-calculator-end)--

Adding the calculator actor type to our config is achieved by calling
``add_actor_type``. After calling this in our config, we can spawn the
``calculator`` anywhere in the distributed actor system (assuming all nodes use
the same config). Note that the handle type still requires a type ID (see
custom-message-types_).

Our final example illustrates how to spawn a ``calculator`` locally by
using its type name. Because the dynamic type name lookup can fail and the
construction arguments passed as message can mismatch, this version of
``spawn`` returns ``expected<T>``.

.. code-block:: C++

   auto x = system.spawn<calculator>("calculator", make_message());
   if (! x) {
     std::cerr << "*** unable to spawn calculator: " << to_string(x.error())
               << std::endl;
     return;
   }
   calculator c = std::move(*x);

Adding dynamically typed actors to the config is achieved in the same way. When
spawning a dynamically typed actor in this way, the template parameter is
simply ``actor``. For example, spawning an actor "foo" which requires
one string is created with:

.. code-block:: C++

   auto worker = system.spawn<actor>("foo", make_message("bar"));

Because constructor (or function) arguments for spawning the actor are stored
in a ``message``, only actors with appropriate input types are allowed.
For example, pointer types are illegal. Hence users need to replace C-strings
with ``std::string``.

.. _log-output:

Log Output
----------

Logging is disabled in CAF per default. It can be enabled by setting the
``--with-log-level=`` option of the ``configure`` script to one
of ``error``, ``warning``, ``info``, ``debug``,
or ``trace`` (from least output to most). Alternatively, setting the
CMake variable ``CAF_LOG_LEVEL`` to one of these values has the same
effect.

All logger-related configuration options listed here and in
system-config-options_ are silently ignored if logging is disabled.

.. _log-output-file-name:

File
~~~~

File output is disabled per default. Setting ``caf.logger.file.verbosity`` to a
valid severity level causes CAF to print log events to the file specified in
``caf.logger.file.path``.

The ``caf.logger.file.path`` may contain one or more of the following
placeholders:

+-----------------+--------------------------------+
| **Variable**    | **Output**                     |
+-----------------+--------------------------------+
| ``[PID]``       | The OS-specific process ID.    |
+-----------------+--------------------------------+
| ``[TIMESTAMP]`` | The UNIX timestamp on startup. |
+-----------------+--------------------------------+
| ``[NODE]``      | The node ID of the CAF system. |
+-----------------+--------------------------------+

.. _log-output-console:

Console
~~~~~~~

Console output is disabled per default. Setting ``caf.logger.console.verbosity``
to a valid severity level causes CAF to print log events to ``std::clog``.

.. _log-output-format-strings:

Format Strings
~~~~~~~~~~~~~~

CAF uses log4j-like format strings for configuring printing of individual
events via ``caf.logger.file.format`` and
``caf.logger.console.format``. Note that format modifiers are not supported
at the moment. The recognized field identifiers are:

+---------------+-----------------------------------------------------------------------------------------------------------+
| **Character** | **Output**                                                                                                |
+---------------+-----------------------------------------------------------------------------------------------------------+
| ``c``         | The category/component.                                                                                   |
+---------------+-----------------------------------------------------------------------------------------------------------+
| ``C``         | Recognized only for historic reasons. Will always print ``null``.                                         |
+---------------+-----------------------------------------------------------------------------------------------------------+
| ``d``         | The date in ISO 8601 format, i.e., ``"YYYY-MM-DDThh:mm:ss"``.                                             |
+---------------+-----------------------------------------------------------------------------------------------------------+
| ``F``         | The file name.                                                                                            |
+---------------+-----------------------------------------------------------------------------------------------------------+
| ``L``         | The line number.                                                                                          |
+---------------+-----------------------------------------------------------------------------------------------------------+
| ``m``         | The user-defined log message.                                                                             |
+---------------+-----------------------------------------------------------------------------------------------------------+
| ``M``         | The name of the current function. For example, the name of ``void ns::foo::bar()`` is printed as ``bar``. |
+---------------+-----------------------------------------------------------------------------------------------------------+
| ``n``         | A newline.                                                                                                |
+---------------+-----------------------------------------------------------------------------------------------------------+
| ``p``         | The priority (severity level).                                                                            |
+---------------+-----------------------------------------------------------------------------------------------------------+
| ``r``         | Elapsed time since starting the application in milliseconds.                                              |
+---------------+-----------------------------------------------------------------------------------------------------------+
| ``t``         | ID of the current thread.                                                                                 |
+---------------+-----------------------------------------------------------------------------------------------------------+
| ``a``         | ID of the current actor (or ``actor0`` when not logging inside an actor).                                 |
+---------------+-----------------------------------------------------------------------------------------------------------+
| ``%``         | A single percent sign.                                                                                    |
+---------------+-----------------------------------------------------------------------------------------------------------+

.. _log-output-filtering:

Filtering
~~~~~~~~~

The two configuration options ``caf.logger.file.excluded-components`` and
``caf.logger.console.excluded-components`` reduce the amount of generated log
events in addition to the minimum severity level. These parameters are lists of
component names that shall be excluded from any output.
