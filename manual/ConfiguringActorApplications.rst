.. _system-config:

Configuring Actor Applications
==============================

CAF configures applications at startup using an ``actor_system_config`` or a
user-defined subclass of that type. The config objects allow users to add custom
types, to load modules (optional components), and to fine-tune the behavior of
loaded modules with command line options or configuration files (see
:ref:`system-config-options`).

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
#. Parse command line arguments and configuration file (if present).
#. Load all modules requested in ``CAF_MAIN``.
#. Create an actor system.
#. Call ``caf_main`` with the actor system and optionally with ``config``.

When implementing the steps performed by ``CAF_MAIN`` by hand, the ``main``
function would resemble the following (pseudo) code:

.. code-block:: C++

  int main(int argc, char** argv) {
    // Create the config.
    actor_system_config cfg;
    // Add runtime-type information for user-defined types.
    cfg.add_message_types<...>();
    // Read CLI options.
    if (auto err = cfg.parse(argc, argv)) {
      std::cerr << "error while parsing CLI and file options: "
                << to_string(err) << std::endl;
      return EXIT_FAILURE;
    }
    // Return immediately if a help text was printed.
    if (cfg.cli_helptext_printed)
      return EXIT_SUCCESS;
    // Load modules.
    cfg.load<...>();
    // Create the actor system.
    actor_system sys{cfg};
    // Run user-defined code.
    caf_main(sys, cfg);
    return EXIT_SUCCESS;
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

.. note::

    Using type ID blocks is optional. Users can also call ``add_message_type``
    for each user-defined type in the constructor of ``my_config``.

.. _system-config-module:

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

The third option is to simply call ``x.load<mod1>()`` on a config
object *before* initializing an actor system with it.

.. _system-config-options:

Command Line Options and INI Configuration Files
------------------------------------------------

CAF organizes program options in categories and parses CLI arguments as well as
INI files. CLI arguments override values in the INI file which override
hard-coded defaults. Users can add any number of custom program options by
implementing a subtype of ``actor_system_config``. The example below
adds three options to the ``global`` category.

.. literalinclude:: /examples/remoting/distributed_calculator.cpp
   :language: C++
   :start-after: --(rst-config-begin)--
   :end-before: --(rst-config-end)--

We create a new ``global`` category in ``custom_options_``.
Each following call to ``add`` then appends individual options to the
category. The first argument to ``add`` is the associated variable. The
second argument is the name for the parameter, optionally suffixed with a
comma-separated single-character short name. The short name is only considered
for CLI parsing and allows users to abbreviate commonly used option names. The
third and final argument to ``add`` is a help text.

The custom ``config`` class allows end users to set the port for the application
to 42 with either ``-p 42`` (short name) or ``--port=42`` (long name). The long
option name is prefixed by the category when using a different category than
``global``. For example, adding the port option to the category ``foo`` means
end users have to type ``--foo.port=42`` when using the long name. Short names
are unaffected by the category, but have to be unique.

Boolean options do not require arguments. The member variable
``server_mode`` is set to ``true`` if the command line contains
either ``--server-mode`` or ``-s``.

The example uses member variables for capturing user-provided settings for
simplicity. However, this is not required. For example,
``add<bool>(...)`` allows omitting the first argument entirely. All
values of the configuration are accessible with ``get_or``. Note that
all global options can omit the ``"global."`` prefix.

CAF adds the program options ``help`` (with short names ``-h``
and ``-?``) as well as ``long-help`` to the ``global``
category.

The default name for the INI file is ``caf-application.ini``. Users can
change the file name and path by passing ``--config-file=<path>`` on the
command line.

INI files are organized in categories. No value is allowed outside of a category
(no implicit ``global`` category). The parses uses the following syntax:

+-------------------------+-----------------------+
| Syntax                  | Type                  |
+=========================+=======================+
| ``key=true``            | Boolean               |
+-------------------------+-----------------------+
| ``key=1``               | Integer               |
+-------------------------+-----------------------+
| ``key=1.0``             | Floating point number |
+-------------------------+-----------------------+
| ``key=1ms``             | Timespan              |
+-------------------------+-----------------------+
| ``key='foo'``           | Atom                  |
+-------------------------+-----------------------+
| ``key="foo"``           | String                |
+-------------------------+-----------------------+
| ``key=[0, 1, ...]``     | List                  |
+-------------------------+-----------------------+
| ``key={a=1, b=2, ...}`` | Dictionary (map)      |
+-------------------------+-----------------------+

The following example INI file lists all standard options in CAF and their
default value. Note that some options such as ``scheduler.max-threads``
are usually detected at runtime and thus have no hard-coded default.

.. literalinclude:: /examples/caf-application.ini
  :language: INI

.. _add-custom-message-type:

Adding Custom Message Types
---------------------------

CAF requires serialization support for all of its message types (see
:ref:`type-inspection`). In addition, CAF also needs a mapping of unique type
names to user-defined types at runtime. This is required to deserialize
arbitrary messages from the network.

The function ``actor_system_config::add_message_type`` adds runtime-type
information for a single type. It takes a template parameter (the message type)
and one function argument (the type name). For example,
``cfg.add_message_type<foo>("foo")`` would add runtime-type information for the
type ``foo``. However, calling ``add_message_type`` for each type individually
is both verbose and prone to error.

For new code, we strongly recommend using the new *type ID blocks*. When setting
the CMake option ``CAF_ENABLE_TYPE_ID_CHECKS`` to ``ON`` (or calling the
``configure`` script with ``--enable-type-id-checks``), CAF raises a static
assertion that prohibits any message type that does not appear in such a type ID
block. When using this API, users can instead call ``add_message_types`` once
per message block. Combined with the type ID checks, this makes sure that the
runtime-type information never runs out of sync when adding new message types.

Passing the type ID blocks to ``CAF_MAIN`` also automates the setup steps for
adding new message types.

A type ID block in CAF starts by calling ``CAF_BEGIN_TYPE_ID_BLOCK``. Inside the
block appear any number of ``CAF_ADD_TYPE_ID`` and ``CAF_ADD_ATOM`` statements.
The type ID block only requires forward declarations. The block ends at
``CAF_END_TYPE_ID_BLOCK``, as shown in the example below.

.. literalinclude:: /examples/custom_type/custom_types_1.cpp
   :language: C++
   :start-after: --(rst-type-id-block-begin)--
   :end-before: --(rst-type-id-block-end)--

.. note::

    The second argument to ``CAF_ADD_TYPE_ID`` (the type) must appear in extra
    parentheses. This unusual syntax is an artifact of argument handling in
    macros. Without the extra set of parentheses, we would not be able to add
    types with commas such as ``std::pair``.

The first argument to all macros is the name of the type ID block. The macro
expands a name ``X`` to ``caf::id_block::X``. In the example above, we can refer
to the custom type ID block with ``caf::id_block::custom_types_1``. To add the
required runtime-type information to CAF, we can either call
``cfg.add_message_types<caf::id_block::custom_types_1>()`` on a config object
pass the ID block to ``CAF_MAIN``:

.. code-block:: C++

    CAF_MAIN(caf::id_block::custom_types_1)

.. note::

    At the point of calling ``CAF_MAIN`` or ``add_message_types``, the compiler
    must have the type declaration plus all ``inspect`` overloads available for
    each type in the type ID block.

.. _add-custom-actor-type:

Adding Custom Actor Types :sup:`experimental`
---------------------------------------------

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
:ref:`add-custom-message-type`).

After adding the actor type to the config, we can spawn our ``calculator`` by
name. Unlike the regular ``spawn`` overloads, this version requires wrapping the
constructor arguments into a ``message`` and the function might fail and thus
returns an ``expected``:

.. code-block:: C++

   if (auto x = system.spawn<calculator>("calculator", make_message())) {
     //  ... do something with *x ...
   } else {
     std::cerr << "*** unable to spawn a calculator: " << to_string(x.error())
               << std::endl;
     // ...
   }

Adding dynamically typed actors to the config is achieved in the same way. When
spawning a dynamically typed actor in this way, the template parameter is
simply ``actor``. For example, spawning an actor "foo" which requires
one string is created with:

.. code-block:: C++

   auto worker = system.spawn<actor>("foo", make_message("bar"));

Because constructor (or function) arguments for spawning the actor are stored in
a ``message``, only actors with appropriate input types are allowed. Pointer
types, for example, are illegal in messages.

.. _log-output:

Log Output
----------

CAF comes with a logger integrated into the actor system. By default, CAF itself
won't emit any log messages. Developers can set the verbosity of CAF itself at
build time by setting the CMake option ``CAF_LOG_LEVEL`` manually or by passing
``--with-log-level=...`` to the ``configure`` script. The available verbosity
levels are (from least to most output):

- ``error``
- ``warning``
- ``info``
- ``debug``
- ``trace``

The logging infrastructure is always available to users, regardless of the
verbosity level of CAF itself.

.. _log-output-file-name:

File Name
~~~~~~~~~

The output file is generated from the template configured by
``logger-file-name``. This template supports the following variables.

+-----------------+--------------------------------+
| Variable        | Output                         |
+=================+================================+
| ``[PID]``       | The OS-specific process ID.    |
+-----------------+--------------------------------+
| ``[TIMESTAMP]`` | The UNIX timestamp on startup. |
+-----------------+--------------------------------+
| ``[NODE]``      | The node ID of the CAF system. |
+-----------------+--------------------------------+

.. _log-output-console:

Console
~~~~~~~

Console output is disabled per default. Setting ``logger-console`` to either
``uncolored`` or ``colored`` prints log events to ``std::clog``. Using the
``colored`` option will print the log events in different colors depending on
the severity level.

.. _log-output-format-strings:

Format Strings
~~~~~~~~~~~~~~

CAF uses log4j-like format strings for configuring printing of individual
events via ``logger-file-format`` and
``logger-console-format``. Note that format modifiers are not supported
at the moment. The recognized field identifiers are:

+---------+-----------------------------------------------------------------------------------------------------------------------+
| Pattern | Output                                                                                                                |
+=========+=======================================================================================================================+
| ``%c``  | The category/component.                                                                                               |
+---------+-----------------------------------------------------------------------------------------------------------------------+
| ``%C``  | The full qualifier of the current function. For example, the function ``void ns::foo::bar()`` would print ``ns.foo``. |
+---------+-----------------------------------------------------------------------------------------------------------------------+
| ``%d``  | The date in ISO 8601 format, i.e., ``"YYYY-MM-DDThh:mm:ss"``.                                                         |
+---------+-----------------------------------------------------------------------------------------------------------------------+
| ``%F``  | The file name.                                                                                                        |
+---------+-----------------------------------------------------------------------------------------------------------------------+
| ``%L``  | The line number.                                                                                                      |
+---------+-----------------------------------------------------------------------------------------------------------------------+
| ``%m``  | The user-defined log message.                                                                                         |
+---------+-----------------------------------------------------------------------------------------------------------------------+
| ``%M``  | The name of the current function. For example, the name of ``void ns::foo::bar()`` is printed as ``bar``.             |
+---------+-----------------------------------------------------------------------------------------------------------------------+
| ``%n``  | A newline.                                                                                                            |
+---------+-----------------------------------------------------------------------------------------------------------------------+
| ``%p``  | The priority (severity level).                                                                                        |
+---------+-----------------------------------------------------------------------------------------------------------------------+
| ``%r``  | Elapsed time since starting the application in milliseconds.                                                          |
+---------+-----------------------------------------------------------------------------------------------------------------------+
| ``%t``  | ID of the current thread.                                                                                             |
+---------+-----------------------------------------------------------------------------------------------------------------------+
| ``%a``  | ID of the current actor (or ``actor0`` when not logging inside an actor).                                             |
+---------+-----------------------------------------------------------------------------------------------------------------------+
| ``%%``  | A single percent sign.                                                                                                |
+---------+-----------------------------------------------------------------------------------------------------------------------+

.. _log-output-filtering:

Filtering
~~~~~~~~~

The two configuration options ``logger.component-blacklist`` and
``logger.(file|console)-verbosity`` reduce the amount of generated log events.
The former is a list of excluded component names and the latter can increase the
reported severity level (but not decrease it beyond the level defined at compile
time).
