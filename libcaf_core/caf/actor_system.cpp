// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/actor_system.hpp"

#include "caf/actor.hpp"
#include "caf/actor_companion.hpp"
#include "caf/actor_from_state.hpp"
#include "caf/actor_ostream.hpp"
#include "caf/actor_registry.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/assert.hpp"
#include "caf/detail/critical.hpp"
#include "caf/detail/meta_object.hpp"
#include "caf/detail/private_thread_pool.hpp"
#include "caf/detail/thread_safe_actor_clock.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/raise_error.hpp"
#include "caf/scheduler.hpp"
#include "caf/spawn_options.hpp"
#include "caf/stateful_actor.hpp"
#include "caf/system_messages.hpp"
#include "caf/telemetry/metric_registry.hpp"

#include <array>
#include <atomic>
#include <cstdio>
#include <fstream>
#include <memory>
#include <unordered_map>
#include <unordered_set>

// -- printer actor ------------------------------------------------------------

namespace caf::detail {

struct printer_actor_state {
  struct actor_data {
    std::string current_line;
    actor_data() {
      // nop
    }
  };

  using data_map = std::unordered_map<actor_id, actor_data>;

  explicit printer_actor_state(event_based_actor* selfptr) : self(selfptr) {
    // nop
  }

  event_based_actor* self;

  data_map data;

  actor_data* get_data(actor_id addr, bool insert_missing) {
    if (addr == invalid_actor_id)
      return nullptr;
    auto i = data.find(addr);
    if (i == data.end() && insert_missing)
      i = data.emplace(addr, actor_data{}).first;
    if (i != data.end())
      return &(i->second);
    return nullptr;
  }

  void flush(actor_data* what, bool forced) {
    if (what == nullptr)
      return;
    auto& line = what->current_line;
    if (line.empty() || (line.back() != '\n' && !forced))
      return;
    self->println(line);
    line.clear();
  }

  behavior make_behavior() {
    return {
      [this](add_atom, actor_id aid, std::string& str) {
        if (str.empty() || aid == invalid_actor_id)
          return;
        auto d = get_data(aid, true);
        if (d != nullptr) {
          d->current_line += str;
          flush(d, false);
        }
      },
      [this](flush_atom, actor_id aid) { flush(get_data(aid, false), true); },
      [this](delete_atom, actor_id aid) {
        auto data_ptr = get_data(aid, false);
        if (data_ptr != nullptr) {
          flush(data_ptr, true);
          data.erase(aid);
        }
      },
      [](redirect_atom, const std::string&, int) {
        // nop
      },
      [](redirect_atom, actor_id, const std::string&, int) {
        // nop
      },
    };
  }

  static constexpr const char* name = "printer_actor";
};

} // namespace caf::detail

namespace caf {

namespace {

struct kvstate {
  using key_type = std::string;
  using mapped_type = message;
  using subscriber_set = std::unordered_set<strong_actor_ptr>;
  using topic_set = std::unordered_set<std::string>;
  std::unordered_map<key_type, std::pair<mapped_type, subscriber_set>> data;
  std::unordered_map<strong_actor_ptr, topic_set> subscribers;
  static inline const char* name = "caf.system.config-server";
};

behavior config_serv_impl(stateful_actor<kvstate>* self) {
  auto lg = log::core::trace("");
  std::string wildcard = "*";
  auto unsubscribe_all = [=](actor subscriber) {
    auto& subscribers = self->state().subscribers;
    auto ptr = actor_cast<strong_actor_ptr>(subscriber);
    auto i = subscribers.find(ptr);
    if (i == subscribers.end())
      return;
    for (auto& key : i->second)
      self->state().data[key].second.erase(ptr);
    subscribers.erase(i);
  };
  self->set_down_handler([=](down_msg& dm) {
    auto lg = log::core::trace("dm = {}", dm);
    auto ptr = actor_cast<strong_actor_ptr>(dm.source);
    if (ptr)
      unsubscribe_all(actor_cast<actor>(std::move(ptr)));
  });
  return {
    // set a key/value pair
    [=](put_atom, const std::string& key, message& msg) {
      auto lg = log::core::trace("key = {}, msg = {}", key, msg);
      if (key == "*")
        return;
      auto& vp = self->state().data[key];
      vp.first = std::move(msg);
      for (auto& subscriber_ptr : vp.second) {
        // we never put a nullptr in our map
        auto subscriber = actor_cast<actor>(subscriber_ptr);
        if (subscriber != self->current_sender())
          self->mail(update_atom_v, key, vp.first).send(subscriber);
      }
      // also iterate all subscribers for '*'
      for (auto& subscriber : self->state().data[wildcard].second)
        if (subscriber != self->current_sender())
          self->mail(update_atom_v, key, vp.first)
            .send(actor_cast<actor>(subscriber));
    },
    // get a key/value pair
    [=](get_atom, std::string& key) -> message {
      auto lg = log::core::trace("key = {}", key);
      if (key == wildcard) {
        std::vector<std::pair<std::string, message>> msgs;
        for (auto& kvp : self->state().data)
          if (kvp.first != "*")
            msgs.emplace_back(kvp.first, kvp.second.first);
        return make_message(std::move(msgs));
      }
      auto i = self->state().data.find(key);
      return make_message(std::move(key), i != self->state().data.end()
                                            ? i->second.first
                                            : make_message());
    },
    // subscribe to a key
    [=](subscribe_atom, const std::string& key) {
      auto subscriber = actor_cast<strong_actor_ptr>(self->current_sender());
      auto lg = log::core::trace("key = {}, subscriber = {}", key, subscriber);
      if (!subscriber)
        return;
      self->state().data[key].second.insert(subscriber);
      auto& subscribers = self->state().subscribers;
      auto i = subscribers.find(subscriber);
      if (i != subscribers.end()) {
        i->second.insert(key);
      } else {
        self->monitor(subscriber);
        subscribers.emplace(subscriber, kvstate::topic_set{key});
      }
    },
    // unsubscribe from a key
    [=](unsubscribe_atom, const std::string& key) {
      auto subscriber = actor_cast<strong_actor_ptr>(self->current_sender());
      if (!subscriber)
        return;
      auto lg = log::core::trace("key = {}, subscriber = {}", key, subscriber);
      if (key == wildcard) {
        unsubscribe_all(actor_cast<actor>(std::move(subscriber)));
        return;
      }
      self->state().subscribers[subscriber].erase(key);
      self->state().data[key].second.erase(subscriber);
    },
    // get a 'named' actor from the local registry
    [=](registry_lookup_atom, const std::string& name) {
      return self->home_system().registry().get(name);
    },
  };
}

// -- spawn server -------------------------------------------------------------

// A spawn server allows users to spawn actors dynamically with a name and a
// message containing the data for initialization. By accessing the spawn server
// on another node, users can spawn actors remotely.

struct spawn_serv_state {
  static inline const char* name = "caf.system.spawn-server";
};

behavior spawn_serv_impl(stateful_actor<spawn_serv_state>* self) {
  auto lg = log::core::trace("");
  return {
    [=](spawn_atom, const std::string& name, message& args,
        actor_system::mpi& xs) -> result<strong_actor_ptr> {
      auto lg = log::core::trace("name = {}, args = {}", name, args);
      return self->system().spawn<strong_actor_ptr>(name, std::move(args),
                                                    self->context(), true, &xs);
    },
  };
}

// -- stream server ------------------------------------------------------------

// The stream server acts as a man-in-the-middle for all streams that cross the
// network. It manages any number of unrelated streams by placing itself and the
// stream server on the next remote node into the pipeline.

// Outgoing messages are buffered in FIFO order to ensure fairness. However, the
// stream server uses five different FIFO queues: on for each priority level.
// A high priority grants more network bandwidth.

// Note that stream servers do not actively take part in the streams they
// process. Batch messages and ACKs are treated equally. Open, close, and error
// messages are evaluated to add and remove state as needed.

auto make_base_metrics(telemetry::metric_registry& reg) {
  return actor_system::base_metrics_t{
    // Initialize the base metrics.
    reg.counter_singleton("caf.system", "rejected-messages",
                          "Number of rejected messages.", "1", true),
    reg.counter_singleton("caf.system", "processed-messages",
                          "Number of processed messages.", "1", true),
    reg.gauge_singleton("caf.system", "running-actors",
                        "Number of currently running actors."),
    reg.gauge_singleton("caf.system", "queued-messages",
                        "Number of messages in all mailboxes.", "1", true),
  };
}

auto make_actor_metric_families(telemetry::metric_registry& reg) {
  // Handling a single message generally should take microseconds. Going up to
  // several milliseconds usually indicates a problem (or blocking operations)
  // but may still be expected for very compute-intense tasks. Single messages
  // that approach seconds to process most likely indicate a severe issue.
  // Hence, the default bucket settings focus on micro- and milliseconds.
  std::array<double, 9> default_buckets{{
    .00001, // 10us
    .0001,  // 100us
    .0005,  // 500us
    .001,   // 1ms
    .01,    // 10ms
    .1,     // 100ms
    .5,     // 500ms
    1.,     // 1s
    5.,     // 5s
  }};
  return actor_system::actor_metric_families_t{
    reg.histogram_family<double>("caf.actor", "processing-time", {"name"},
                                 default_buckets,
                                 "Time an actor needs to process messages.",
                                 "seconds"),
    reg.histogram_family<double>(
      "caf.actor", "mailbox-time", {"name"}, default_buckets,
      "Time a message waits in the mailbox before processing.", "seconds"),
    reg.gauge_family("caf.actor", "mailbox-size", {"name"},
                     "Number of messages in the mailbox."),
    {
      reg.counter_family("caf.actor.stream", "processed-elements",
                         {"name", "type"},
                         "Number of processed stream elements from upstream."),
      reg.gauge_family("caf.actor.stream", "input-buffer-size",
                       {"name", "type"},
                       "Number of buffered stream elements from upstream."),
      reg.counter_family(
        "caf.actor.stream", "pushed-elements", {"name", "type"},
        "Number of elements that have been pushed downstream."),
      reg.gauge_family("caf.actor.stream", "output-buffer-size",
                       {"name", "type"},
                       "Number of buffered output stream elements."),
    },
  };
}

class print_state_impl {
public:
  using print_fun = void (*)(void*, term, const char*, size_t);

  using cleanup = void (*)(void*);

  explicit print_state_impl(const actor_system_config& cfg) {
    auto colored = get_or(cfg, "caf.console.colored", true);
    auto out = get_or(cfg, "caf.console.stream", "stdout");
    // Short-circuit if output is disabled.
    if (out == "none") {
      do_print_ = [](void*, term, const char*, size_t) {};
      return;
    }
    // Pick the output stream.
    if (out == "stderr")
      out_ = stderr;
    else
      out_ = stdout;
    // Set up our print function depending on whether we want colored output.
    if (colored && detail::is_tty(stdout)) {
      do_print_ = [](void* out, term color, const char* buf, size_t len) {
        auto* fhdl = reinterpret_cast<FILE*>(out);
        if (color <= term::reset_endl) {
          fwrite(buf, 1, len, fhdl);
          return;
        }
        detail::set_color(fhdl, color);
        fwrite(buf, 1, len, fhdl);
        detail::set_color(fhdl, term::reset);
      };
    } else {
      do_print_ = [](void* out, term, const char* buf, size_t len) {
        fwrite(buf, 1, len, reinterpret_cast<FILE*>(out));
      };
    }
  }

  ~print_state_impl() {
    if (do_cleanup_)
      do_cleanup_(out_);
  }

  void reset(void* new_out, print_fun do_print, cleanup do_cleanup) {
    std::lock_guard guard{mtx_};
    if (do_cleanup_)
      do_cleanup_(out_);
    out_ = new_out;
    do_print_ = do_print;
    do_cleanup_ = do_cleanup;
  }

  void print(term color, const char* buf, size_t len) {
    std::lock_guard guard{mtx_};
    do_print_(out_, color, buf, len);
  }

private:
  /// Mutex for printing to the console.
  std::mutex mtx_;

  /// File descriptor for printing to the console.
  void* out_ = nullptr;

  /// Function for printing to the console.
  print_fun do_print_ = nullptr;

  /// Function for cleaning up out_.
  cleanup do_cleanup_ = nullptr;
};

} // namespace

class actor_system::impl {
public:
  using module_ptr = std::unique_ptr<actor_system_module>;

  using module_array = std::array<module_ptr, actor_system_module::num_ids>;

  impl(actor_system* parent, actor_system_config& cfg,
       custom_setup_fn custom_setup, void* custom_setup_data)
    : ids(0),
      metrics(cfg),
      base_metrics(make_base_metrics(metrics)),
      registry(*parent),
      await_actors_before_shutdown(true),
      cfg(&cfg),
      private_threads(parent) {
    print_state = std::make_unique<print_state_impl>(cfg);
    meta_objects_guard = detail::global_meta_objects_guard();
    if (!meta_objects_guard)
      CAF_CRITICAL("unable to obtain the global meta objects guard");
    for (auto& hook : cfg.thread_hooks())
      hook->init(*parent);
    // Cache some configuration parameters for faster lookups at runtime.
    using string_list = std::vector<std::string>;
    if (auto lst = get_as<string_list>(cfg,
                                       "caf.metrics-filters.actors.includes"))
      metrics_actors_includes = std::move(*lst);
    if (auto lst = get_as<string_list>(cfg,
                                       "caf.metrics-filters.actors.excludes"))
      metrics_actors_excludes = std::move(*lst);
    if (!metrics_actors_includes.empty())
      actor_metric_families = make_actor_metric_families(metrics);
    // Spin up modules.
    for (auto fn : cfg.module_factories()) {
      auto mod_ptr = fn(*parent);
      auto mod_id = mod_ptr->id();
      modules[mod_id].reset(mod_ptr);
    }
    // Make sure meta objects are loaded.
    auto gmos = detail::global_meta_objects();
    if (gmos.size() < id_block::core_module::end
        || gmos[id_block::core_module::begin].type_name.empty()) {
      CAF_CRITICAL("actor_system created without calling "
                   "caf::init_global_meta_objects<>() before");
    }
    if (modules[actor_system_module::middleman] != nullptr) {
      if (gmos.size() < detail::io_module_end
          || gmos[detail::io_module_begin].type_name.empty()) {
        CAF_CRITICAL("I/O module loaded without calling "
                     "caf::io::middleman::init_global_meta_objects() before");
      }
    }
    // Allow the callback to override any configuration parameter and to
    // initialize member variables before we set the defaults.
    if (custom_setup != nullptr) {
      custom_setup(*parent, cfg, custom_setup_data);
    }
    // Initialize the logger before any other module.
    if (!logger) {
      logger = logger::make(*parent);
      logger->init(cfg);
      CAF_SET_LOGGER_SYS(parent);
    }
    // Make sure we have a clock.
    if (!clock) {
      clock = std::make_unique<detail::thread_safe_actor_clock>(*parent);
    }
    // Make sure we have a scheduler up and running.
    if (!scheduler) {
      using defaults::scheduler::policy;
      auto config_policy = get_or(cfg, "caf.scheduler.policy", policy);
      if (config_policy == "sharing") {
        scheduler = scheduler::make_work_sharing(*parent);
      } else {
        // Any invalid configuration falls back to work stealing.
        if (config_policy != "stealing")
          fprintf(stderr,
                  "[WARNING] '%s' is an unrecognized scheduler policy, falling "
                  "back to 'stealing' (i.e. work-stealing)\n",
                  config_policy.c_str());
        scheduler = scheduler::make_work_stealing(*parent);
      }
    }
    scheduler->start();
    if (!printer) {
      auto p = parent->spawn<hidden + detached + lazy_init>(
        actor_from_state<detail::printer_actor_state>);
      printer = actor_cast<strong_actor_ptr>(std::move(p));
    }
    // Initialize the state for each module and give each module the opportunity
    // to adapt the system configuration.
    for (auto& mod : modules)
      if (mod)
        mod->init(cfg);
    // Spawn config and spawn servers (lazily to not access the scheduler yet).
    static constexpr auto Flags = hidden + lazy_init;
    spawn_serv
      = actor_cast<strong_actor_ptr>(parent->spawn<Flags>(spawn_serv_impl));
    config_serv
      = actor_cast<strong_actor_ptr>(parent->spawn<Flags>(config_serv_impl));
    // Start all modules.
    registry.start();
    private_threads.start();
    registry.put("SpawnServ", parent->spawn_serv());
    registry.put("ConfigServ", parent->config_serv());
    for (auto& mod : modules)
      if (mod)
        mod->start();
    logger->start();
  }

  ~impl() {
    {
      auto lg = log::core::trace("");
      log::core::debug("shutdown actor system");
      if (await_actors_before_shutdown)
        registry.await_running_count_equal(0);
      // shutdown internal actors
      auto drop = [&](auto& x) {
        anon_send_exit(x, exit_reason::user_shutdown);
        x = nullptr;
      };
      drop(spawn_serv);
      drop(config_serv);
      // stop modules in reverse order
      for (auto i = modules.rbegin(); i != modules.rend(); ++i) {
        auto& ptr = *i;
        if (ptr != nullptr) {
          log::core::debug("stop module {}", ptr->name());
          ptr->stop();
        }
      }
      drop(printer);
      CAF_LOG_DEBUG("stop scheduler");
      scheduler->stop();
      private_threads.stop();
      registry.stop();
      clock = nullptr;
    }
    // reset logger and wait until dtor was called
    CAF_SET_LOGGER_SYS(nullptr);
    logger->stop();
    logger = nullptr;
  }

  /// Used to generate ascending actor IDs.
  std::atomic<size_t> ids;

  /// Manages all metrics collected by the system.
  telemetry::metric_registry metrics;

  /// Stores all metrics that the actor system collects by default.
  base_metrics_t base_metrics;

  /// Identifies this actor system in a distributed setting.
  node_id node;

  /// Maps well-known actor names to actor handles.
  actor_registry registry;

  /// Manages log output.
  intrusive_ptr<caf::logger> logger;

  /// Stores the system-wide clock.
  std::unique_ptr<actor_clock> clock;

  /// Stores the actor system scheduler.
  std::unique_ptr<caf::scheduler> scheduler;

  /// Stores optional actor system components.
  module_array modules;

  /// Background printer.
  strong_actor_ptr printer;

  /// Stores whether the system should wait for running actors on shutdown.
  bool await_actors_before_shutdown;

  /// Stores config parameters.
  strong_actor_ptr config_serv;

  /// Allows fully dynamic spawning of actors.
  strong_actor_ptr spawn_serv;

  /// The system-wide, user-provided configuration.
  actor_system_config* cfg;

  /// Caches the configuration parameter `caf.metrics-filters.actors.includes`
  /// for faster lookups at runtime.
  std::vector<std::string> metrics_actors_includes;

  /// Caches the configuration parameter `caf.metrics-filters.actors.excludes`
  /// for faster lookups at runtime.
  std::vector<std::string> metrics_actors_excludes;

  /// Caches families for optional actor metrics.
  actor_metric_families_t actor_metric_families;

  /// Manages threads for detached actors.
  detail::private_thread_pool private_threads;

  /// Ties the lifetime of the meta objects table to the actor system.
  detail::global_meta_objects_guard_type meta_objects_guard;

  std::unique_ptr<print_state_impl> print_state;
};

actor_system::networking_module::~networking_module() {
  // nop
}

actor_system::actor_system(actor_system_config& cfg, version::abi_token token)
  : actor_system(cfg, nullptr, nullptr, token) {
  // nop
}

actor_system::actor_system(actor_system_config& cfg,
                           custom_setup_fn custom_setup,
                           void* custom_setup_data, version::abi_token token) {
  // Make sure the ABI token matches the expected version.
  if (static_cast<int>(token) != CAF_VERSION_MAJOR) {
    CAF_CRITICAL("CAF ABI token mismatch");
  }
  // This is a convoluted way to construct the implementation, but we cannot
  // just use new and delete here. The `custom_setup` function might call member
  // functions on this actor system when constructing `impl`, which requires us
  // to have `impl_` already pointing to the new instance. Hence, we first
  // allocate memory for `impl_` and then construct the implementation object.
  impl_ = static_cast<impl*>(malloc(sizeof(impl)));
  new (impl_) impl(this, cfg, custom_setup, custom_setup_data);
}

actor_system::~actor_system() {
  impl_->~impl();
  free(impl_);
}

// -- properties ---------------------------------------------------------------

actor_system::base_metrics_t& actor_system::base_metrics() noexcept {
  return impl_->base_metrics;
}

const actor_system::base_metrics_t&
actor_system::base_metrics() const noexcept {
  return impl_->base_metrics;
}

const actor_system::actor_metric_families_t&
actor_system::actor_metric_families() const noexcept {
  return impl_->actor_metric_families;
}

detail::global_meta_objects_guard_type
actor_system::meta_objects_guard() const noexcept {
  return impl_->meta_objects_guard;
}

span<const std::string> actor_system::metrics_actors_includes() const noexcept {
  return impl_->metrics_actors_includes;
}

span<const std::string> actor_system::metrics_actors_excludes() const noexcept {
  return impl_->metrics_actors_excludes;
}

const actor_system_config& actor_system::config() const {
  return *impl_->cfg;
}

actor_clock& actor_system::clock() noexcept {
  return *impl_->clock;
}

size_t actor_system::detached_actors() const noexcept {
  return impl_->private_threads.running();
}

bool actor_system::await_actors_before_shutdown() const {
  return impl_->await_actors_before_shutdown;
}

void actor_system::await_actors_before_shutdown(bool new_value) {
  impl_->await_actors_before_shutdown = new_value;
}

const strong_actor_ptr& actor_system::spawn_serv() const {
  return impl_->spawn_serv;
}

const strong_actor_ptr& actor_system::config_serv() const {
  return impl_->config_serv;
}

telemetry::metric_registry& actor_system::metrics() noexcept {
  return impl_->metrics;
}

const telemetry::metric_registry& actor_system::metrics() const noexcept {
  return impl_->metrics;
}

const node_id& actor_system::node() const {
  return impl_->node;
}

caf::scheduler& actor_system::scheduler() {
  return *impl_->scheduler;
}

caf::logger& actor_system::logger() {
  return *impl_->logger;
}

actor_registry& actor_system::registry() {
  return impl_->registry;
}

bool actor_system::has_middleman() const {
  return impl_->modules[actor_system_module::middleman] != nullptr;
}

io::middleman& actor_system::middleman() {
  if (auto& clptr = impl_->modules[actor_system_module::middleman])
    return *reinterpret_cast<io::middleman*>(clptr->subtype_ptr());
  CAF_RAISE_ERROR("cannot access middleman: module not loaded");
}

bool actor_system::has_openssl_manager() const {
  return impl_->modules[actor_system_module::openssl_manager] != nullptr;
}

openssl::manager& actor_system::openssl_manager() const {
  if (auto& clptr = impl_->modules[actor_system_module::openssl_manager])
    return *reinterpret_cast<openssl::manager*>(clptr->subtype_ptr());
  CAF_RAISE_ERROR("cannot access middleman: module not loaded");
}

bool actor_system::has_network_manager() const noexcept {
  return impl_->modules[actor_system_module::network_manager] != nullptr;
}

net::middleman& actor_system::network_manager() {
  if (auto& clptr = impl_->modules[actor_system_module::network_manager])
    return *reinterpret_cast<net::middleman*>(clptr->subtype_ptr());
  CAF_RAISE_ERROR("cannot access network manager: module not loaded");
}

actor_id actor_system::next_actor_id() {
  return ++impl_->ids;
}

actor_id actor_system::latest_actor_id() const {
  return impl_->ids.load();
}

strong_actor_ptr actor_system::legacy_printer_actor() const {
  return impl_->printer;
}

void actor_system::await_all_actors_done() const {
  impl_->registry.await_running_count_equal(0);
}

void actor_system::monitor(const node_id& node, const actor_addr& observer) {
  // TODO: Currently does not work with other modules, in particular caf_net.
  auto mm = impl_->modules[actor_system_module::middleman].get();
  if (mm == nullptr)
    return;
  static_cast<networking_module*>(mm)->monitor(node, observer);
}

void actor_system::demonitor(const node_id& node, const actor_addr& observer) {
  // TODO: Currently does not work with other modules, in particular caf_net.
  auto mm = impl_->modules[actor_system_module::middleman].get();
  if (mm == nullptr)
    return;
  auto mm_dptr = static_cast<networking_module*>(mm);
  mm_dptr->demonitor(node, observer);
}

intrusive_ptr<actor_companion> actor_system::make_companion() {
  actor_config cfg;
  cfg.mbox_factory = mailbox_factory();
  auto hdl = spawn_class<actor_companion, no_spawn_options>(cfg);
  auto* ptr = actor_cast<abstract_actor*>(hdl);
  return intrusive_ptr<actor_companion>{static_cast<actor_companion*>(ptr)};
}

void actor_system::thread_started(thread_owner owner) {
  for (auto& hook : impl_->cfg->thread_hooks())
    hook->thread_started(owner);
}

void actor_system::thread_terminates() {
  for (auto& hook : impl_->cfg->thread_hooks())
    hook->thread_terminates();
}

expected<strong_actor_ptr>
actor_system::dyn_spawn_impl(const std::string& name, message& args,
                             caf::scheduler* sched, bool check_interface,
                             const mpi* expected_ifs) {
  auto lg = log::core::trace(
    "name = {}, args = {}, check_interface = {}, expected_ifs = {}", name, args,
    check_interface, expected_ifs);
  if (name.empty())
    return sec::invalid_argument;
  auto* fs = impl_->cfg->get_actor_factory(name);
  if (fs == nullptr)
    return sec::unknown_type;
  actor_config cfg{sched != nullptr ? sched : &scheduler()};
  auto res = (*fs)(*this, cfg, args);
  if (!res.first)
    return sec::cannot_spawn_actor_from_arguments;
  if (check_interface && !assignable(res.second, *expected_ifs))
    return sec::unexpected_actor_messaging_interface;
  return std::move(res.first);
}

detail::private_thread* actor_system::acquire_private_thread() {
  return impl_->private_threads.acquire();
}

void actor_system::release_private_thread(detail::private_thread* ptr) {
  impl_->private_threads.release(ptr);
}

namespace {

class actor_local_printer_impl : public detail::actor_local_printer {
public:
  actor_local_printer_impl(abstract_actor* self, actor printer)
    : self_(self->id()), printer_(std::move(printer)) {
    CAF_ASSERT(printer_ != nullptr);
    if (!self->getf(abstract_actor::has_used_aout_flag))
      self->setf(abstract_actor::has_used_aout_flag);
  }

  void write(std::string&& arg) override {
    printer_->enqueue(make_mailbox_element(nullptr, make_message_id(),
                                           add_atom_v, self_, std::move(arg)),
                      nullptr);
  }

  void write(const char* arg) override {
    write(std::string{arg});
  }

  void flush() override {
    printer_->enqueue(make_mailbox_element(nullptr, make_message_id(),
                                           flush_atom_v, self_),
                      nullptr);
  }

private:
  actor_id self_;
  actor printer_;
};

} // namespace

detail::actor_local_printer_ptr actor_system::printer_for(local_actor* self) {
  using impl_t = actor_local_printer_impl;
  return make_counted<impl_t>(self, actor_cast<actor>(impl_->printer));
}

detail::mailbox_factory* actor_system::mailbox_factory() {
  return impl_->cfg->mailbox_factory();
}

void actor_system::redirect_text_output(void* out,
                                        void (*write)(void*, term, const char*,
                                                      size_t),
                                        void (*cleanup)(void*)) {
  impl_->print_state->reset(out, write, cleanup);
}

void actor_system::do_print(term color, const char* buf, size_t num_bytes) {
  impl_->print_state->print(color, buf, num_bytes);
}

// -- callbacks for actor_system_access ----------------------------------------

void actor_system::set_logger(intrusive_ptr<caf::logger> ptr) {
  impl_->logger = std::move(ptr);
  impl_->logger->init(config());
  CAF_SET_LOGGER_SYS(this);
}

void actor_system::set_clock(std::unique_ptr<actor_clock> ptr) {
  impl_->clock = std::move(ptr);
}

void actor_system::set_scheduler(std::unique_ptr<caf::scheduler> ptr) {
  impl_->scheduler = std::move(ptr);
}

void actor_system::set_legacy_printer_actor(strong_actor_ptr ptr) {
  impl_->printer = std::move(ptr);
}

void actor_system::set_node(node_id id) {
  impl_->node = id;
}

} // namespace caf
