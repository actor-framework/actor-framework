// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/io/middleman.hpp"

#include "caf/io/basp/header.hpp"
#include "caf/io/basp_broker.hpp"
#include "caf/io/network/default_multiplexer.hpp"

#include "caf/actor.hpp"
#include "caf/actor_registry.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/after.hpp"
#include "caf/anon_mail.hpp"
#include "caf/config.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/latch.hpp"
#include "caf/detail/prometheus_broker.hpp"
#include "caf/function_view.hpp"
#include "caf/init_global_meta_objects.hpp"
#include "caf/log/system.hpp"
#include "caf/logger.hpp"
#include "caf/node_id.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/sec.hpp"
#include "caf/send.hpp"
#include "caf/thread_owner.hpp"

#include <cstring>
#include <memory>

#ifdef CAF_WINDOWS
#  include <fcntl.h>
#  include <io.h>
#endif // CAF_WINDOWS

namespace caf::io {

namespace {

auto make_metrics(telemetry::metric_registry& reg) {
  std::array<double, 9> default_time_buckets{{
    .0002, //  20us
    .0004, //  40us
    .0006, //  60us
    .0008, //  80us
    .001,  //   1ms
    .005,  //   5ms
    .01,   //  10ms
    .05,   //  50ms
    .1,    // 100ms
  }};
  std::array<int64_t, 9> default_size_buckets{{
    100,
    500,
    1'000,
    5'000,
    10'000,
    50'000,
    100'000,
    500'000,
    1'000'000,
  }};
  return middleman::metric_singletons_t{
    reg.histogram_singleton(
      "caf.middleman", "inbound-messages-size", default_size_buckets,
      "The size of inbound messages before deserializing them.", "bytes"),
    reg.histogram_singleton<double>(
      "caf.middleman", "deserialization-time", default_time_buckets,
      "Time the middleman needs to deserialize inbound messages.", "seconds"),
    reg.histogram_singleton(
      "caf.middleman", "outbound-messages-size", default_size_buckets,
      "The size of outbound messages after serializing them.", "bytes"),
    reg.histogram_singleton<double>(
      "caf.middleman", "serialization-time", default_time_buckets,
      "Time the middleman needs to serialize outbound messages.", "seconds"),
  };
}

template <class T>
class mm_impl : public middleman {
public:
  mm_impl(actor_system& ref) : middleman(ref), backend_(&ref) {
    // nop
  }

  network::multiplexer& backend() override {
    return backend_;
  }

private:
  T backend_;
};

class prometheus_scraping : public middleman::background_task {
public:
  prometheus_scraping(actor_system& sys) : mpx_(&sys) {
    // nop
  }

  expected<uint16_t> start(const config_value::dictionary& cfg) {
    // Read port, address and reuse flag from the config.
    uint16_t port = 0;
    if (auto cfg_port = get_as<uint16_t>(cfg, "port")) {
      port = *cfg_port;
    } else {
      return false;
    }
    const char* addr = nullptr;
    if (const std::string* cfg_addr = get_if<std::string>(&cfg, "address"))
      if (*cfg_addr != "" && *cfg_addr != "0.0.0.0")
        addr = cfg_addr->c_str();
    return start(port, addr, get_or(cfg, "reuse", false));
  }

  expected<uint16_t> start(uint16_t port, const char* in, bool reuse) {
    doorman_ptr dptr;
    if (auto maybe_dptr = mpx_.new_tcp_doorman(port, in, reuse)) {
      dptr = std::move(*maybe_dptr);
    } else {
      auto& err = maybe_dptr.error();
      log::system::error("failed to expose Prometheus metrics: {}", err);
      return std::move(err);
    }
    auto actual_port = dptr->port();
    using impl = detail::prometheus_broker;
    mpx_supervisor_ = mpx_.make_supervisor();
    actor_config cfg{&mpx_};
    broker_ = mpx_.system().spawn_impl<impl, hidden>(cfg, std::move(dptr));
    detail::latch sync{1};
    auto run_mpx = [this, sync_ptr{&sync}] {
      auto exit_guard = log::io::trace("");
      mpx_.thread_id(std::this_thread::get_id());
      sync_ptr->count_down();
      mpx_.run();
    };
    thread_ = mpx_.system().launch_thread("caf.io.prom", thread_owner::system,
                                          run_mpx);
    sync.wait();
    log::io::info("expose Prometheus metrics at port {}", actual_port);
    return actual_port;
  }

  ~prometheus_scraping() {
    if (mpx_supervisor_) {
      mpx_.dispatch([this] {
        auto ptr = static_cast<broker*>(actor_cast<abstract_actor*>(broker_));
        if (!ptr->getf(abstract_actor::is_terminated_flag)) {
          ptr->context(&mpx_);
          ptr->quit();
          ptr->finalize();
        }
      });
      mpx_supervisor_.reset();
      thread_.join();
    }
  }

private:
  network::default_multiplexer mpx_;
  network::multiplexer::supervisor_ptr mpx_supervisor_;
  actor broker_;
  std::thread thread_;
};

} // namespace

middleman::background_task::~background_task() {
  // nop
}

void middleman::init_global_meta_objects() {
  caf::init_global_meta_objects<id_block::io_module>();
}

void middleman::add_module_options(actor_system_config& cfg) {
  // Add options to the CLI parser.
  config_option_adder{cfg.custom_options(), "caf.middleman"}
    .add<std::vector<std::string>>("app-identifiers",
                                   "valid application identifiers of this node")
    .add<bool>("enable-automatic-connections",
               "enables automatic connection management")
    .add<size_t>("max-consecutive-reads",
                 "max. number of consecutive reads per broker")
    .add<timespan>("heartbeat-interval", "interval of heartbeat messages")
    .add<timespan>("connection-timeout",
                   "max. time between messages before declaring a node dead "
                   "(disabled if 0, ignored if heartbeats are disabled)")
    .add<bool>("attach-utility-actors",
               "schedule utility actors instead of dedicating threads")
    .add<size_t>("workers", "number of deserialization workers");
  config_option_adder{cfg.custom_options(), "caf.middleman.prometheus-http"}
    .add<uint16_t>("port", "listening port for incoming scrapes")
    .add<std::string>("address", "bind address for the HTTP server socket");
  // Add the defaults to the config so they show up in --dump-config.
  auto& grp = put_dictionary(cfg.content, "caf.middleman");
  auto default_id = std::string{defaults::middleman::app_identifier};
  put_missing(grp, "app-identifiers",
              std::vector<std::string>{std::move(default_id)});
  put_missing(grp, "enable-automatic-connections", false);
  put_missing(grp, "max-consecutive-reads",
              defaults::middleman::max_consecutive_reads);
  put_missing(grp, "heartbeat-interval",
              defaults::middleman::heartbeat_interval);
  put_missing(grp, "connection-timeout",
              defaults::middleman::connection_timeout);
}

actor_system::module* middleman::make(actor_system& sys, type_list<>) {
  return new mm_impl<network::default_multiplexer>(sys);
}

middleman::middleman(actor_system& sys) : system_(sys) {
  metric_singletons = make_metrics(sys.metrics());
}

expected<strong_actor_ptr>
middleman::remote_spawn_impl(const node_id& nid, std::string& name,
                             message& args, std::set<std::string> s,
                             timespan timeout) {
  auto f = make_function_view(actor_handle(), timeout);
  return f(spawn_atom_v, nid, std::move(name), std::move(args), std::move(s));
}

expected<uint16_t> middleman::open(uint16_t port, const char* in, bool reuse) {
  std::string str;
  if (in != nullptr)
    str = in;
  auto f = make_function_view(actor_handle());
  return f(open_atom_v, port, std::move(str), reuse);
}

expected<void> middleman::close(uint16_t port) {
  auto f = make_function_view(actor_handle());
  return f(close_atom_v, port);
}

expected<node_id> middleman::connect(std::string host, uint16_t port) {
  auto f = make_function_view(actor_handle());
  auto res = f(connect_atom_v, std::move(host), port);
  if (!res)
    return std::move(res.error());
  return std::get<0>(*res);
}

expected<uint16_t> middleman::publish(const strong_actor_ptr& whom,
                                      std::set<std::string> sigs, uint16_t port,
                                      const char* cstr, bool ru) {
  auto exit_guard = log::io::trace("whom = {}, sigs = {}, port = {}", whom,
                                   sigs, port);
  if (!whom)
    return sec::cannot_publish_invalid_actor;
  std::string in;
  if (cstr != nullptr)
    in = cstr;
  auto f = make_function_view(actor_handle());
  return f(publish_atom_v, port, std::move(whom), std::move(sigs), in, ru);
}

expected<void> middleman::unpublish(const actor_addr& whom, uint16_t port) {
  auto exit_guard = log::io::trace("whom = {}, port = {}", whom, port);
  auto f = make_function_view(actor_handle());
  return f(unpublish_atom_v, whom, port);
}

expected<strong_actor_ptr> middleman::remote_actor(std::set<std::string> ifs,
                                                   std::string host,
                                                   uint16_t port) {
  auto exit_guard = log::io::trace("ifs = {}, host = {}, port = {}", ifs, host,
                                   port);
  auto f = make_function_view(actor_handle());
  auto res = f(connect_atom_v, std::move(host), port);
  if (!res)
    return std::move(res.error());
  strong_actor_ptr ptr = std::move(std::get<1>(*res));
  if (!ptr)
    return make_error(sec::no_actor_published_at_port, port);
  if (!system().assignable(std::get<2>(*res), ifs))
    return make_error(sec::unexpected_actor_messaging_interface, std::move(ifs),
                      std::move(std::get<2>(*res)));
  return ptr;
}

strong_actor_ptr middleman::remote_lookup(std::string name,
                                          const node_id& nid) {
  auto exit_guard = log::io::trace("name = {}, nid = {}", name, nid);
  if (system().node() == nid)
    return system().registry().get(name);
  auto basp = named_broker<basp_broker>("BASP");
  strong_actor_ptr result;
  scoped_actor self{system(), true};
  auto id = basp::header::config_server_id;
  self
    ->mail(forward_atom_v, nid, id,
           make_message(registry_lookup_atom_v, std::move(name)))
    .send(basp);
  self->receive([&](strong_actor_ptr& addr) { result = std::move(addr); },
                [](message& msg) {
                  log::system::error(
                    "received unexpected remote_lookup result: {}", msg);
                },
                after(std::chrono::minutes(5)) >> [] { //
                  log::io::warning("remote_lookup timed out");
                });
  return result;
}

void middleman::start() {
  auto exit_guard = log::io::trace("");
  // Consider using net::middleman for prometheus if caf-net is available.
  if (auto prom = get_if<config_value::dictionary>(
        &system().config(), "caf.middleman.prometheus-http")) {
    auto ptr = std::make_unique<prometheus_scraping>(system());
    if (auto port = ptr->start(*prom)) {
      CAF_ASSERT(*port != 0);
      prometheus_scraping_port_ = *port;
      background_tasks_.emplace_back(std::move(ptr));
    }
  }
  // Launch backend.
  backend_supervisor_ = backend().make_supervisor();
  CAF_ASSERT(backend_supervisor_ != nullptr);
  detail::latch sync{1};
  auto run_backend = [this, sync_ptr{&sync}] {
    auto exit_guard = log::io::trace("");
    backend().thread_id(std::this_thread::get_id());
    sync_ptr->count_down();
    backend().run();
  };
  thread_ = system().launch_thread("caf.io.mpx", thread_owner::system,
                                   run_backend);
  sync.wait();
  // Spawn utility actors.
  auto basp = named_broker<basp_broker>("BASP");
  manager_ = make_middleman_actor(system(), basp);
}

void middleman::stop() {
  auto exit_guard = log::io::trace("");
  backend().dispatch([this] {
    auto exit_guard = log::io::trace("");
    // managers_ will be modified while we are stopping each manager,
    // because each manager will call remove(...)
    for (auto& kvp : named_brokers_) {
      auto& hdl = kvp.second;
      auto ptr = static_cast<broker*>(actor_cast<abstract_actor*>(hdl));
      if (!ptr->getf(abstract_actor::is_terminated_flag)) {
        ptr->context(&backend());
        ptr->quit();
        ptr->finalize();
      }
    }
  });
  backend_supervisor_.reset();
  if (thread_.joinable())
    thread_.join();
  named_brokers_.clear();
  scoped_actor self{system(), true};
  self->send_exit(manager_, exit_reason::kill);
  if (!get_or(config(), "caf.middleman.attach-utility-actors", false))
    self->wait_for(manager_);
  destroy(manager_);
  background_tasks_.clear();
}

void middleman::init(actor_system_config& cfg) {
  // Compute and set ID for this network node.
  auto this_node = node_id::default_data::local(cfg);
  system().node_.swap(this_node);
}

actor_system::module::id_t middleman::id() const {
  return module::middleman;
}

void* middleman::subtype_ptr() {
  return this;
}

void middleman::monitor(const node_id& node, const actor_addr& observer) {
  auto basp = named_broker<basp_broker>("BASP");
  anon_mail(monitor_atom_v, node, observer).send(basp);
}

void middleman::demonitor(const node_id& node, const actor_addr& observer) {
  auto basp = named_broker<basp_broker>("BASP");
  anon_mail(demonitor_atom_v, node, observer).send(basp);
}

middleman::~middleman() {
  // nop
}

middleman_actor middleman::actor_handle() {
  return manager_;
}

} // namespace caf::io
