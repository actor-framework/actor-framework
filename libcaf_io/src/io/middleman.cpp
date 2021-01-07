// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/io/middleman.hpp"

#include <cerrno>
#include <cstring>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <tuple>

#include "caf/actor.hpp"
#include "caf/actor_proxy.hpp"
#include "caf/actor_registry.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/after.hpp"
#include "caf/config.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/get_mac_addresses.hpp"
#include "caf/detail/get_root_uuid.hpp"
#include "caf/detail/prometheus_broker.hpp"
#include "caf/detail/ripemd_160.hpp"
#include "caf/detail/safe_equal.hpp"
#include "caf/detail/set_thread_name.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/function_view.hpp"
#include "caf/group_module.hpp"
#include "caf/init_global_meta_objects.hpp"
#include "caf/io/basp/header.hpp"
#include "caf/io/basp_broker.hpp"
#include "caf/io/network/default_multiplexer.hpp"
#include "caf/io/network/interfaces.hpp"
#include "caf/io/network/test_multiplexer.hpp"
#include "caf/io/system_messages.hpp"
#include "caf/logger.hpp"
#include "caf/make_counted.hpp"
#include "caf/node_id.hpp"
#include "caf/others.hpp"
#include "caf/scheduler/abstract_coordinator.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/sec.hpp"
#include "caf/send.hpp"
#include "caf/typed_event_based_actor.hpp"

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

  bool start(const config_value::dictionary& cfg) override {
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
    auto reuse = get_or(cfg, "reuse", false);
    if (auto res = start(port, addr, reuse)) {
      CAF_LOG_INFO("expose Prometheus metrics at port" << *res);
      return true;
    } else {
      CAF_LOG_ERROR("failed to expose Prometheus metrics:" << res.error());
      return false;
    }
  }

  expected<uint16_t> start(uint16_t port, const char* in, bool reuse) {
    doorman_ptr dptr;
    if (auto maybe_dptr = mpx_.new_tcp_doorman(port, in, reuse))
      dptr = std::move(*maybe_dptr);
    else
      return std::move(maybe_dptr.error());
    auto actual_port = dptr->port();
    // Spawn the actor and store its handle in background_brokers_.
    using impl = detail::prometheus_broker;
    actor_config cfg{&mpx_};
    broker_ = mpx_.system().spawn_impl<impl, hidden>(cfg, std::move(dptr));
    thread_ = std::thread{[this] { mpx_.run(); }};
    return actual_port;
  }

  ~prometheus_scraping() {
    if (broker_) {
      anon_send_exit(broker_, exit_reason::user_shutdown);
      thread_.join();
    }
  }

private:
  network::default_multiplexer mpx_;
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
  config_option_adder{cfg.custom_options(), "caf.middleman"}
    .add<std::string>("network-backend",
                      "either 'default' or 'asio' (if available)")
    .add<std::vector<std::string>>("app-identifiers",
                                   "valid application identifiers of this node")
    .add<bool>("enable-automatic-connections",
               "enables automatic connection management")
    .add<size_t>("max-consecutive-reads",
                 "max. number of consecutive reads per broker")
    .add<timespan>("heartbeat-interval", "interval of heartbeat messages")
    .add<bool>("attach-utility-actors",
               "schedule utility actors instead of dedicating threads")
    .add<bool>("manual-multiplexing",
               "disables background activity of the multiplexer")
    .add<size_t>("workers", "number of deserialization workers");
  config_option_adder{cfg.custom_options(), "caf.middleman.prometheus-http"}
    .add<uint16_t>("port", "listening port for incoming scrapes")
    .add<std::string>("address", "bind address for the HTTP server socket");
}

actor_system::module* middleman::make(actor_system& sys, detail::type_list<>) {
  auto impl = get_or(sys.config(), "caf.middleman.network-backend",
                     defaults::middleman::network_backend);
  if (impl == "testing")
    return new mm_impl<network::test_multiplexer>(sys);
  else
    return new mm_impl<network::default_multiplexer>(sys);
}

middleman::middleman(actor_system& sys) : system_(sys) {
  remote_groups_ = make_counted<detail::remote_group_module>(this);
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
  CAF_LOG_TRACE(CAF_ARG(whom) << CAF_ARG(sigs) << CAF_ARG(port));
  if (!whom)
    return sec::cannot_publish_invalid_actor;
  std::string in;
  if (cstr != nullptr)
    in = cstr;
  auto f = make_function_view(actor_handle());
  return f(publish_atom_v, port, std::move(whom), std::move(sigs), in, ru);
}

expected<uint16_t> middleman::publish_local_groups(uint16_t port,
                                                   const char* in, bool reuse) {
  CAF_LOG_TRACE(CAF_ARG(port) << CAF_ARG(in));
  auto group_nameserver = [](event_based_actor* self) -> behavior {
    return {
      [self](get_atom, const std::string& id) {
        auto grp = self->system().groups().get_local(id);
        return grp.get()->intermediary();
      },
    };
  };
  auto ns = system().spawn<hidden + lazy_init>(group_nameserver);
  if (auto result = publish(ns, port, in, reuse)) {
    manager_->add_link(actor_cast<abstract_actor*>(ns));
    return *result;
  } else {
    anon_send_exit(ns, exit_reason::user_shutdown);
    return std::move(result.error());
  }
}

expected<void> middleman::unpublish(const actor_addr& whom, uint16_t port) {
  CAF_LOG_TRACE(CAF_ARG(whom) << CAF_ARG(port));
  auto f = make_function_view(actor_handle());
  return f(unpublish_atom_v, whom, port);
}

expected<strong_actor_ptr> middleman::remote_actor(std::set<std::string> ifs,
                                                   std::string host,
                                                   uint16_t port) {
  CAF_LOG_TRACE(CAF_ARG(ifs) << CAF_ARG(host) << CAF_ARG(port));
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

expected<group> middleman::remote_group(const std::string& group_uri) {
  CAF_LOG_TRACE(CAF_ARG(group_uri));
  // format of group_identifier is group@host:port
  // a regex would be the natural choice here, but we want to support
  // older compilers that don't have <regex> implemented (e.g. GCC < 4.9)
  auto pos1 = group_uri.find('@');
  auto pos2 = group_uri.find(':');
  auto last = std::string::npos;
  if (pos1 == last || pos2 == last || pos1 >= pos2)
    return make_error(sec::invalid_argument, "invalid URI format", group_uri);
  auto name = group_uri.substr(0, pos1);
  auto host = group_uri.substr(pos1 + 1, pos2 - pos1 - 1);
  auto port = static_cast<uint16_t>(std::stoi(group_uri.substr(pos2 + 1)));
  return remote_group(name, host, port);
}

expected<group> middleman::remote_group(const std::string& group_identifier,
                                        const std::string& host,
                                        uint16_t port) {
  CAF_LOG_TRACE(CAF_ARG(group_identifier) << CAF_ARG(host) << CAF_ARG(port));
  // Helper actor that first connects to the remote actor at `host:port` and
  // then tries to get a valid group from that actor.
  auto two_step_lookup = [=](event_based_actor* self, middleman_actor mm,
                             detail::remote_group_module_ptr rgm) -> behavior {
    return {
      [=](get_atom) {
        /// We won't receive a second message, so we drop our behavior here to
        /// terminate the actor after both requests finish.
        self->unbecome();
        auto rp = self->make_response_promise();
        self->request(mm, infinite, connect_atom_v, host, port)
          .then([=](const node_id&, strong_actor_ptr& ptr,
                    const std::set<std::string>&) mutable {
            CAF_LOG_DEBUG("received handle to target node:" << CAF_ARG(ptr));
            auto hdl = actor_cast<actor>(ptr);
            self->request(hdl, infinite, get_atom_v, group_identifier)
              .then([=](actor intermediary) mutable {
                CAF_LOG_DEBUG("lookup successful:" << CAF_ARG(group_identifier)
                                                   << CAF_ARG(intermediary));
                auto ptr = rgm->get_impl(intermediary, group_identifier);
                auto result = group{ptr.get()};
                rp.deliver(std::move(result));
              });
          });
      },
    };
  };
  // Spawn the helper actor and wait for the result.
  expected<group> result{sec::cannot_connect_to_node};
  scoped_actor self{system(), true};
  auto worker = self->spawn<lazy_init + monitored>(two_step_lookup,
                                                   actor_handle(),
                                                   remote_groups_);
  self->send(worker, get_atom_v);
  self->receive(
    [&](group& grp) {
      CAF_LOG_DEBUG("received remote group handle:" << CAF_ARG(grp));
      result = std::move(grp);
    },
    [&](error& err) {
      CAF_LOG_DEBUG("received an error while waiting for the group:" << err);
      result = std::move(err);
    },
    [&](down_msg& dm) {
      CAF_LOG_DEBUG("lookup actor failed:" << CAF_ARG(dm));
      result = std::move(dm.reason);
    });
  return result;
}

void middleman::resolve_remote_group_intermediary(
  const node_id& origin, const std::string& group_identifier,
  std::function<void(actor)> callback) {
  auto lookup = [=, cb{std::move(callback)}](event_based_actor* self,
                                             middleman_actor mm) {
    self
      ->request(mm, std::chrono::minutes(1), get_atom_v, group_atom_v, origin,
                group_identifier)
      .then([cb](actor& hdl) { cb(std::move(hdl)); },
            [cb](const error&) { cb(actor{}); });
  };
  system().spawn(lookup, actor_handle());
}

strong_actor_ptr middleman::remote_lookup(std::string name,
                                          const node_id& nid) {
  CAF_LOG_TRACE(CAF_ARG(name) << CAF_ARG(nid));
  if (system().node() == nid)
    return system().registry().get(name);
  auto basp = named_broker<basp_broker>("BASP");
  strong_actor_ptr result;
  scoped_actor self{system(), true};
  auto id = basp::header::config_server_id;
  self->send(basp, forward_atom_v, nid, id,
             make_message(registry_lookup_atom_v, std::move(name)));
  self->receive(
    [&](strong_actor_ptr& addr) { result = std::move(addr); },
    others >> [](message& msg) -> skippable_result {
      CAF_LOG_ERROR(
        "middleman received unexpected remote_lookup result:" << msg);
      return message{};
    },
    after(std::chrono::minutes(5)) >>
      [&] { CAF_LOG_WARNING("remote_lookup for" << name << "timed out"); });
  return result;
}

void middleman::start() {
  CAF_LOG_TRACE("");
  // Launch background tasks.
  if (auto prom = get_if<config_value::dictionary>(
        &system().config(), "caf.middleman.prometheus-http")) {
    auto ptr = std::make_unique<prometheus_scraping>(system());
    if (ptr->start(*prom))
      background_tasks_.emplace_back(std::move(ptr));
  }
  // Launch backend.
  if (!get_or(config(), "caf.middleman.manual-multiplexing", false))
    backend_supervisor_ = backend().make_supervisor();
  // The only backend that returns a `nullptr` by default is the
  // `test_multiplexer` which does not have its own thread but uses the main
  // thread instead. Other backends can set `middleman_detach_multiplexer` to
  // false to suppress creation of the supervisor.
  if (backend_supervisor_ != nullptr) {
    std::atomic<bool> init_done{false};
    std::mutex mtx;
    std::condition_variable cv;
    thread_ = std::thread{[&, this] {
      CAF_SET_LOGGER_SYS(&system());
      detail::set_thread_name("caf.multiplexer");
      system().thread_started();
      CAF_LOG_TRACE("");
      {
        std::unique_lock<std::mutex> guard{mtx};
        backend().thread_id(std::this_thread::get_id());
        init_done = true;
        cv.notify_one();
      }
      backend().run();
      system().thread_terminates();
    }};
    std::unique_lock<std::mutex> guard{mtx};
    while (init_done == false)
      cv.wait(guard);
  }
  // Spawn utility actors.
  auto basp = named_broker<basp_broker>("BASP");
  manager_ = make_middleman_actor(system(), basp);
  // Enable deserialization of groups.
  system().groups().get_remote
    = [this](const node_id& origin, const std::string& module_name,
             const std::string& group_identifier) -> expected<group> {
    if (module_name == "local" || module_name == "remote") {
      auto ptr = remote_groups_->get_impl(origin, group_identifier);
      return group{ptr.get()};
    } else {
      return make_error(
        sec::runtime_error,
        "currently, only 'local' groups are accessible remotely");
    }
  };
}

void middleman::stop() {
  CAF_LOG_TRACE("");
  backend().dispatch([=] {
    CAF_LOG_TRACE("");
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
  if (!get_or(config(), "caf.middleman.manual-multiplexing", false)) {
    backend_supervisor_.reset();
    if (thread_.joinable())
      thread_.join();
  } else {
    while (backend().try_run_once())
      ; // nop
  }
  named_brokers_.clear();
  scoped_actor self{system(), true};
  self->send_exit(manager_, exit_reason::kill);
  if (!get_or(config(), "caf.middleman.attach-utility-actors", false))
    self->wait_for(manager_);
  destroy(manager_);
  background_tasks_.clear();
}

void middleman::init(actor_system_config& cfg) {
  // Note: logging is not available at this stage.
  // Never detach actors when using the testing multiplexer.
  auto network_backend = get_or(cfg, "caf.middleman.network-backend",
                                defaults::middleman::network_backend);
  if (network_backend == "testing") {
    cfg.set("caf.middleman.attach-utility-actors", true)
      .set("caf.middleman.manual-multiplexing", true);
  }
  // Compute and set ID for this network node.
  auto this_node = node_id::default_data::local(cfg);
  system().node_.swap(this_node);
  // Give config access to slave mode implementation.
  cfg.slave_mode_fun = &middleman::exec_slave_mode;
  // Enable users to use 'remote:foo@bar' notation for remote groups.
  auto dummy_fac = [ptr{remote_groups_}]() -> group_module* {
    auto raw = ptr.get();
    raw->ref();
    return raw;
  };
  cfg.group_module_factories.emplace_back(dummy_fac);
}

actor_system::module::id_t middleman::id() const {
  return module::middleman;
}

void* middleman::subtype_ptr() {
  return this;
}

void middleman::monitor(const node_id& node, const actor_addr& observer) {
  auto basp = named_broker<basp_broker>("BASP");
  anon_send(basp, monitor_atom_v, node, observer);
}

void middleman::demonitor(const node_id& node, const actor_addr& observer) {
  auto basp = named_broker<basp_broker>("BASP");
  anon_send(basp, demonitor_atom_v, node, observer);
}

middleman::~middleman() {
  // nop
}

middleman_actor middleman::actor_handle() {
  return manager_;
}

int middleman::exec_slave_mode(actor_system&, const actor_system_config&) {
  // TODO
  return 0;
}

} // namespace caf::io
