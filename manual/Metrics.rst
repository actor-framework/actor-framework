.. _metrics:

Metrics
=======

Building and testing an application (or microservice) is merely the first step
in its lifetime cycle. Once you enter production and start deploying your
software, you constantly need to monitor it. Is it still running? How many
actors do we have? How much requests can our system handle? Where are potential
bottlenecks? Do we have resources to spare or do we need to allocate more? Are
we keeping our SLAs?

In order to answer such high-level questions, powerful tools like `Prometheus
<https://prometheus.io>`_ have emerged. However, such monitoring systems are
only as good as the data you feed it.

The metrics API in CAF enables you to instrument your code for generating
performance data. The API is vendor-neutral, but borrows many concepts as well
as terminology from Prometheus. Currently, CAF can only export metrics to
Prometheus. However, the API allows users to collect the metrics manually for
writing custom integrations.

.. note::

  All classes for instrumenting code live in the namespace ``caf::telemetry``.

Metric Names and Labels
-----------------------

Each metric is uniquely identified by:

- A prefix. This acts as a namespace for grouping metrics together. All metrics
  that CAF collects by itself use the prefix ``caf``.
- A name. This identifies the metric within the prefix. By convention, these
  names are all-lowercase and hyphenated. For example, ``running-actors``.
- Any number of label dimensions. Labels are key-value pairs that divide a
  metric into useful categories. For example, a metric that counts HTTP requests
  could split into ``method=get``, ``method=put``, ``method=post``, etc.
  Aggregating all metrics by ``method`` would then yield the total amount.

Metrics that share prefix, name and label names form a *metric family*. This is
also directly reflected in the API: the class ``metric_family`` bundles all
shared attributes and stores all instances as children.

A metric family without labels always contains exactly one child. Hence, CAF
calls this metric *singleton* in its API.

.. note::

  CAF identifies metrics by prefix and name. Hence, families with the same
  prefix and name but different label names are prohibited.

Metric Types
------------

CAF knows these types of metrics:

#. **Counters**. A counter represents a monotonically increasing value. For
   example, the total number of messages received by all actors, the total
   number of errors since starting the system, etc.
#. **Gauges**. A gauge represents a numerical value that can arbitrarily
   increase or decrease. For example, the current number of messages in all
   mailboxes, the number of running actors, etc.
#. **Histograms**. A histogram observes numerical values and counts them in
   (configurable) buckets. For example, sampling the processing time of messages
   ``t`` with buckets for ``0ms ≤ t ≤ 1ms``, ``1ms < t ≤ 10ms``, ``10ms < t ≤
   100ms``, and so on gives information on the usual response time and outliers.
   Histograms internally consist of counters and provide a relatively
   lightweight sampling mechanism. However, providing the right boundaries for
   the buckets can require some experimentation or experience.

Further, CAF provides two implementations for each metric type: one using
``int64_t`` as internal representation and one using ``double``. Both
implementations use atomic operations, but the former is usually more efficient
on platforms such as x86. In user code, we recommend only using these type
definitions:

- ``dbl_counter`` for monotonically increasing floating point numbers
- ``int_counter`` for monotonically increasing 64-bit integers
- ``dbl_gauge`` for arbitrary floating point numbers
- ``int_gauge`` for arbitrary 64-bit integers
- ``dbl_histogram`` for sampling floating point numbers
- ``int_histogram`` for sampling 64-bit integers

The associated headers are:

- ``caf/telemetry/counter.hpp``
- ``caf/telemetry/gauge.hpp``
- ``caf/telemetry/histogram.hpp``

Counters
~~~~~~~~

Counters wrap an atomic count but only allows incrementing it. The class
provides the following member functions:

.. code-block:: C++

  /// Increments the counter by 1.
  void inc() noexcept;

  /// Increments the counter by `amount`.
  /// @pre `amount > 0`
  void inc(value_type amount) noexcept;

  /// Returns the current value of the counter.
  value_type value() const noexcept;

  /// Increments the counter by 1.
  /// @note only available if value_type == int64_t
  value_type operator++() noexcept;

Gauges
~~~~~~

Like counters, gauges also wrap an atomic count. However, gauges are less
permissive and allow decrementing as well.

.. code-block:: C++

  /// Increments the gauge by 1.
  void inc() noexcept;

  /// Increments the gauge by `amount`.
  void inc(value_type amount) noexcept;

  /// Decrements the gauge by 1.
  void dec() noexcept;

  /// Decrements the gauge by `amount`.
  void dec(value_type amount) noexcept;

  /// Sets the gauge to `x`.
  void value(value_type x) noexcept;

  /// Increments the gauge by 1.
  /// @returns The new value of the gauge.
  /// @note only available if value_type == int64_t
  value_type operator++() noexcept;

  /// Decrements the gauge by 1.
  /// @returns The new value of the gauge.
  /// @note only available if value_type == int64_t
  value_type operator--() noexcept;

  /// Returns the current value of the gauge.
  value_type value() const noexcept;

Histogram
~~~~~~~~~

Histograms consist of one counter per bucket as well as a gauge for the sum of
all observed values (values may be negative).

.. code-block:: C++

  /// Increments the bucket where the observed value falls into and increments
  /// the sum of all observed values.
  void observe(value_type value);

  /// Returns the sum of all observed values.
  value_type sum() const noexcept;

Metric Units and Flags
----------------------

All metric types store numerical values, either as ``double`` or as ``int64_t``.
For giving this number additional semantics, CAF allows assigning *units* (of
measurement) to metrics. The default unit is ``1``, which denotes dimensionless
counts such as the number of messages in a mailbox.

The unit can be any string, but we recommend using only *base units* such as
``seconds`` or ``bytes`` to make processing of these metrics with monitoring
systems easier.

Each metric also carries one flag: ``is-sum``. Setting this to ``true`` (the
default is ``false``) indicates that this metric adds something up to a total
where only the total value is of interest. For example, the total number of HTTP
requests. CAF itself does not care about the flag, but it can give extra
information to collectors or exporters. For example, the Prometheus exporter
will add a ``_total`` suffix to the exported metric name.

The Metric Registry
-------------------

All metrics of an actor system are managed by a single registry to make sure
only one metric instance exists per prefix and name combination. Further, the
registry stores all metrics in a single place to allow *collectors* to iterate
over all metrics in a single place.

A minimal custom collector class requires providing ``operator()`` overloads as
shown below:

.. code-block:: C++

  class my_collector {
  public:
    void operator()(const metric_family* family, const metric* instance,
                    const dbl_counter* impl);

    void operator()(const metric_family* family, const metric* instance,
                    const int_counter* impl);

    void operator()(const metric_family* family, const metric* instance,
                    const dbl_gauge* impl);

    void operator()(const metric_family* family, const metric* instance,
                    const int_gauge* impl);

    void operator()(const metric_family* family, const metric* instance,
                    const dbl_histogram* impl);

    void operator()(const metric_family* family, const metric* instance,
                    const int_histogram* impl);
  };

Applying the collector to the registry looks as follows (with ``sys`` being a
reference to an ``actor_system``):

.. code-block:: C++

  my_collector f;
  sys.metrics().collect(f);

The associated headers is ``caf/telemetry/metric_registry.hpp``.


Accessing Metrics
-----------------

Accessing a metric is a three-step process:

1. Get the ``metric_registry`` from the actor system.
2. Get the ``metric_family`` from the registry.
3. Call ``get_or_add`` on the family to get a pointer to the counter, gauge, or
   histogram.

The pointer remains valid until the actor system gets destroyed. Hence, holding
on to the pointer in an actor is always safe.

The registry creates metrics lazily (to be more precise, it creates families
lazily that in turn create metric instances lazily). Since this requires
synchronization via mutexes, we recommend to only access the registry once per
metric and then store the pointer.

Accessing Counters and Gauges
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Counters and gauges are very similar in their API. Hence, all functions that
work on gauges only require replacing ``gauge`` with ``counter`` to work with
counters instead.

Gauges are owned (and created) by a gauge family object. We can either get the
family object explicitly by calling ``gauge_family``, or we can use one of the
two shortcut functions ``gauge_intance`` or ``gauge_singleton``. The C++
prototypes for the registry member functions look as follows:

.. code-block:: C++

  template <class ValueType = int64_t>
  auto* gauge_family(string_view prefix, string_view name,
                     span<const string_view> labels, string_view helptext,
                     string_view unit = "1", bool is_sum = false);

  template <class ValueType = int64_t>
  auto* gauge_instance(string_view prefix, string_view name,
                       span<const label_view> labels, string_view helptext,
                       string_view unit = "1", bool is_sum = false);

  template <class ValueType = int64_t>
  auto* gauge_singleton(string_view prefix, string_view name,
                        string_view helptext, string_view unit = "1",
                        bool is_sum = false);

.. note::

  All functions that take a ``span`` also provide an overload that accepts a
  ``std::initializer_list`` instead to make working with constants easier.

The function ``gauge_family`` returns a type-specific metric family object,
while the other two functions return the gauge directly.

The family objects only have a single noteworthy member function,
``get_or_add``:

.. code-block:: C++


  auto fptr = registry.counter_family("http", "requests", {"method"},
                                      "Number of HTTP requests.", "seconds",
                                      true);
  auto count = fptr->get_or_add({{"method", "put"}});

If we only get a single counter from the family, we can use ``counter_instance``
instead:

.. code-block:: C++

  auto count = registry.counter_instance("http", "requests",
                                         {{"method", "put"}},
                                         "Number of HTTP requests.",
                                         "seconds", true);

Accessing Histograms
~~~~~~~~~~~~~~~~~~~~

The member functions for accessing histogram families and histograms follow the
same pattern as the member functions for counters and gauges.

.. code-block:: C++

  template <class ValueType = int64_t>
  auto* histogram_family(string_view prefix, string_view name,
                         span<const string_view> label_names,
                         span<const ValueType> default_upper_bounds,
                         string_view helptext, string_view unit = "1",
                         bool is_sum = false);

  template <class ValueType = int64_t>
  auto* histogram_instance(string_view prefix, string_view name,
                           span<const label_view> label_names,
                           span<const ValueType> default_upper_bounds,
                           string_view helptext, string_view unit = "1",
                           bool is_sum = false);

  template <class ValueType = int64_t>
  auto* histogram_singleton(string_view prefix, string_view name,
                            span<const ValueType> default_upper_bounds,
                            string_view helptext, string_view unit = "1",
                            bool is_sum = false);

Compared to the member functions for counters and guages, histograms require one
addition argument for the default bucket upper bounds.

.. warning::

  The ``default_upper_bounds`` parameter **must** be sorted!

CAF automatically adds one additional bucket for observing all values between
the last upper bound and *infinity* (``double``) or *INT_MAX* (``int64_t``). For
example, passing ``[10, 100, 1000]`` as upper bounds creates four buckets in
total. The first bucket captues all values with ``x ≤ 10``. The second bucket
captues all values with ``10 < x ≤ 100``. The third bucket captures all values
with ``100 < x ≤ 1000``. Finally, the fourth bucket (added automatically)
captures all values with ``1000 < x ≤ INT_MAX``.

Configuration Parameters
------------------------

Histograms use the actor system configuration to enable users to override
hard-coded default bucket settings. On construction, the histogram family check
whether a key ``caf.metrics.${prefix}.${name}.buckets`` exists. Further, the
metric instance also checks on construction whether a more specific bucket
setting for one of its label dimensions exist.

For example, consider we add a histogram family with prefix ``http``, name
``request-duration``, and label dimension ``method`` to the registry. The family
first tries to read ``caf.metrics.http.request-duration.buckets`` from the
configuration and otherwise falls back to the hard-coded defaults. When creating
a histogram instance from the family with the label ``method=put``, the
construct first tries to read
``caf.metrics.http.request-duration.method=put.buckets`` from the configuration
and otherwise uses the default for the family.

In a configuration file, users may provide bucket settings like this:

.. code-block:: none

  caf {
    metrics {
      http {
        # measures the duration per HTTP request in seconds
        request-duration {
          buckets = [
            0.001, # ≤   1ms
            0.01,  # ≤  10ms
            0.05,  # ≤  50ms
            0.1,   # ≤ 100ms
            0.25,  # ≤ 250ms
            0.5,   # ≤ 500ms
            0.75,  # ≤ 750ms
          ]
          # use different settings for get requests
          "method=put" {
            buckets = [
              0.007, # ≤   7ms
              0.012, # ≤  12ms
              0.025, # ≤  25ms
              0.05,  # ≤  50ms
              0.1,   # ≤ 100ms
            ]
          }
        }
      }
    }
  }

.. note::

  Ambiguous settings for metrics with multiple label dimensions will result in
  CAF picking the first match from an unspecified order. Hence, prefer using
  only one label dimension for configuring buckets or otherwise make sure there
  is always exactly one match for instance labels.

Performance Considerations
--------------------------

Instrumenting code should affect the performance as little as possible. Keep in
mind that each member function on the registry has to acquire a lock. Ideally,
applications call functions such as ``gauge_family`` *once* during setup and
then store the family pointer to create metric instances later.

Ideally, there is a single occurrence in the code for getting the family object
from the registry and a single occurrence in the code for getting the
gauge/counter/histogram object from the family (``get_or_add`` also has to
acquire a lock).

All operations on gauges, counters and histograms use atomic operations.
Depending on the type, CAF internally uses ``std::atomic<int64_t>`` or
``std::atomic<double>``. Adding a sample to a histogram requires two atomic
operations: one for the bucket and one for the sum.

Atomic operations are reasonably fast, but we still recommend to avoid them in
tight loops.

Builtin Metrics
---------------

CAF collects a set of builtin metrics in order to provide insights into the
actor system and its modules. Some are always collect while others require
configuration by the user.

Base Metrics
~~~~~~~~~~~~

The actor system collects this set of metrics always by default.

caf.running-actors
  - Tracks the current number of running actors in the system.
  - **Type**: ``int_gauge``
  - **Label dimensions**: none.

caf.processed-messages
  - Counts the total number of processed messages.
  - **Type**: ``int_counter``
  - **Label dimensions**: none.

caf.rejected-messages
  - Counts the number of messages that where rejected because the target mailbox
    was closed or did not exist.
  - **Type**: ``int_counter``
  - **Label dimensions**: none.

Actor Metrics and Filters
~~~~~~~~~~~~~~~~~~~~~~~~~

Unlike the base metrics, actor metrics are *off* by default. Applications can
spawn thousands of actors, with many only existing for a brief time. Hence,
blindly collecting data from all actors in the system can impact the performance
and also produce a lot of irrelevant noise.

To make sure CAF only collects actor metrics that are relevant to the user, the
actor system configuration provides two lists:
``caf.metrics-filters.actors.includes`` and
``caf.metrics-filters.actors.excludes``. CAF collects metrics for all actors
that have names that are selected by the ``includes`` list and are not selected
by the ``excludes`` list. Entries in the list can use glob-style syntax, in
particular ``*``-wildcards. For example:

.. code-block:: none

  caf {
    metrics-filters {
      actors {
        includes = [ "foo.*" ]
        excludes = [ "foo.bar" ]
      }
    }
  }

The configuration above would select all actors with names that start with
``foo.`` except for actors named ``foo.bar``.

.. note::

  Names belong to actor *types*. CAF assigns default names such as
  ``user.scheduled-actor`` by default. To provide a custom name, either override
  the member function ``const char* name() const`` when implementing class-based
  actors or add a *static* member variable
  ``static inline const char* name = "..."`` to your state class when using
  stateful actors.

  CAF uses a hierarchical, hyphenated naming scheme with ``.`` as the separator
  and all-lowercase name components. For example, ``caf.system.spawn-server``.
  Users may follow this naming scheme for consistency, but CAF does not enforce
  any structure on the names. However, we do recommend to avoid whitespaces and
  special characters that the glob engine recognizes, such as ``*``, ``/``, etc.

For all actors that are selected by the user-defined filters, CAF collects this
set of metrics:

caf.processing-time
  - Samples how long the actor needs to process messages.
  - **Type**: ``dbl_histogram``
  - **Unit**: ``seconds``
  - **Label dimensions**: name.

caf.mailbox-time
  - Samples how long messages wait in the mailbox before being processed.
  - **Type**: ``dbl_histogram``
  - **Unit**: ``seconds``
  - **Label dimensions**: name.

caf.mailbox-size
  - Counts how many messages are currently waiting in the mailbox.
  - **Type**: ``int_gauge``
  - **Label dimensions**: name.

caf.stream.processed-elements
  - Counts the total number of processed stream elements from upstream.
  - **Type**: ``int_counter``
  - **Label dimensions**: name, type.

caf.stream.input-buffer-size
  - Tracks how many stream elements from upstream are currently buffered.
  - **Type**: ``int_gauge``
  - **Label dimensions**: name, type.

caf.stream.pushed-elements
  - Counts the total number of elements that have been pushed downstream.
  - **Type**: ``int_counter``
  - **Label dimensions**: name, type.

caf.stream.output-buffer-size
  - Tracks how many stream elements are currently waiting in the output buffer.
  - **Type**: ``int_gauge``
  - **Label dimensions**: name, type.

Exporting Metrics to Prometheus
-------------------------------

The network module in CAF comes with builtin support for exporting metrics to
Prometheus via HTTP. However, this feature is off by default since CAF generally
avoids opening ports without explicit user input.

During startup, the middleman enables the export of metrics when the
configuration provides a valid value (0 to 65536) for
``caf.middleman.prometheus-http.port`` as shown in the example config file
below.

.. code-block:: none

  caf {
    middleman {
      prometheus-http {
        # listen for incoming HTTP requests on port 8080 (required parameter)
        port = 8080
        # the bind address (optional parameter; default is 0.0.0.0)
        address = "0.0.0.0"
      }
    }
  }
