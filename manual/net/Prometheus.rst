.. _net-prometheus:

Prometheus
==========

CAF ships a Prometheus scraper implementation that allows exposing metrics from
the actor system. User can create an instance of the scraper with the factory
function ``caf::net::prometheus::scraper`` (include ``caf/net/prometheus.hpp``).
The scraper is designed to work with an HTTP server.

A minimal example for starting an HTTP server (see :ref:`net-http`) at port 8081
that responds to GET requests on ``/metrics`` could look like this:

.. code-block:: C++

  auto server = net::http::with(sys)
                  .accept(8081)
                  .route("/metrics", net::prometheus::scraper(sys))
                  .start();

.. note::

  Usually, users  can simply use the configuration options of the actor system
  to export metrics: :ref:`metrics_export`. When setting these options, CAF uses
  this implementation to start the Prometheus server in the background.
