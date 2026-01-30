// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/actor_system.hpp"

#include "caf/actor.hpp"
#include "caf/actor_companion.hpp"
#include "caf/actor_registry.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/actor_system_access.hpp"
#include "caf/detail/assert.hpp"
#include "caf/detail/asynchronous_logger.hpp"
#include "caf/detail/critical.hpp"
#include "caf/detail/daemons.hpp"
#include "caf/detail/match_wildcard_pattern.hpp"
#include "caf/detail/meta_object.hpp"
#include "caf/detail/panic.hpp"
#include "caf/detail/private_thread_pool.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/log/core.hpp"
#include "caf/log/system.hpp"
#include "caf/raise_error.hpp"
#include "caf/scheduler.hpp"
#include "caf/spawn_options.hpp"
#include "caf/stateful_actor.hpp"
#include "caf/system_messages.hpp"
#include "caf/telemetry/metric_registry.hpp"
#include "caf/thread_owner.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <fstream>
#include <limits>
#include <memory>
#include <mutex>
#include <span>
#include <unordered_map>
#include <unordered_set>

namespace caf {

namespace {

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

/// Metrics that the actor system collects.
struct base_metrics_t {
  /// Counts the number of messages that were rejected because the target
  /// mailbox was closed or did not exist.
  telemetry::int_counter* rejected_messages = nullptr;

  /// Counts the total number of messages that wait in a mailbox.
  telemetry::int_gauge* queued_messages = nullptr;

  /// Counts the number of actors that are currently running.
  telemetry::int_gauge_family* running_count = nullptr;

  /// Counts the total number of processed messages by actor type.
  telemetry::int_counter_family* processed_messages = nullptr;

  /// Samples how long the actor needs to process messages by actor type.
  telemetry::dbl_histogram_family* processing_time = nullptr;

  /// Samples how long a message waits in the mailbox before the actor
  /// processes it.
  telemetry::dbl_histogram_family* mailbox_time = nullptr;

  /// Counts how many messages are currently waiting in the mailbox.
  telemetry::int_gauge_family* mailbox_size = nullptr;
};

auto make_base_metrics(telemetry::metric_registry& reg) {
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
  return base_metrics_t{
    .rejected_messages = reg.counter_singleton(
      "caf.system", "rejected-messages", "Number of rejected messages."),
    .queued_messages = reg.gauge_singleton(
      "caf.system", "queued-messages", "Number of messages in all mailboxes."),
    .running_count = reg.gauge_family("caf.system", "running-actors", {"name"},
                                      "Number of currently running actors."),
    .processed_messages = reg.counter_family("caf.actor", "processed-messages",
                                             {"name"},
                                             "Number of processed messages."),
    .processing_time = reg.histogram_family<double>(
      "caf.actor", "processing-time", {"name"}, default_buckets,
      "Time an actor needs to process messages.", "seconds"),
    .mailbox_time = reg.histogram_family<double>(
      "caf.actor", "mailbox-time", {"name"}, default_buckets,
      "Time a message waits in the mailbox before processing.", "seconds"),
    .mailbox_size = reg.gauge_family("caf.actor", "mailbox-size", {"name"},
                                     "Number of messages in the mailbox.")};
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
        auto* fhdl = reinterpret_cast<FILE*>(out);
        fwrite(buf, 1, len, fhdl);
        fflush(fhdl);
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

class actor_registry_impl : public actor_registry {
public:
  using exclusive_guard = std::unique_lock<std::shared_mutex>;
  using shared_guard = std::shared_lock<std::shared_mutex>;

  void erase(actor_id key) override {
    // Stores a reference to the actor we're going to remove. This guarantees
    // that we aren't releasing the last reference to an actor while erasing it.
    // Releasing the final ref can trigger the actor to call its cleanup
    // function that in turn calls this function and we can end up in a
    // deadlock.
    strong_actor_ptr ref;
    { // Lifetime scope of guard.
      exclusive_guard guard{instances_mtx_};
      auto i = entries_.find(key);
      if (i != entries_.end()) {
        ref.swap(i->second);
        entries_.erase(i);
      }
    }
  }

  /// Removes a name mapping.
  void erase(const std::string& key) override {
    // Stores a reference to the actor we're going to remove for the same
    // reasoning as in erase(actor_id).
    strong_actor_ptr ref;
    { // Lifetime scope of guard.
      exclusive_guard guard{named_entries_mtx_};
      auto i = named_entries_.find(key);
      if (i != named_entries_.end()) {
        ref.swap(i->second);
        named_entries_.erase(i);
      }
    }
  }

  using name_map = std::unordered_map<std::string, strong_actor_ptr>;

  name_map named_actors() const override {
    shared_guard guard{named_entries_mtx_};
    return named_entries_;
  }

  // Starts this component.
  void start() {
    // nop
  }

  // Stops this component.
  void stop() {
    {
      exclusive_guard guard{instances_mtx_};
      entries_.clear();
    }
    {
      exclusive_guard guard{named_entries_mtx_};
      named_entries_.clear();
    }
  }

private:
  strong_actor_ptr get_impl(actor_id key) const override {
    shared_guard guard(instances_mtx_);
    auto i = entries_.find(key);
    if (i != entries_.end())
      return i->second;
    log::core::debug("key invalid, assume actor no longer exists: key = {}",
                     key);
    return nullptr;
  }

  void put_impl(actor_id key, strong_actor_ptr val) override {
    auto lg = log::core::trace("key = {}", key);
    if (!val)
      return;
    { // lifetime scope of guard
      exclusive_guard guard(instances_mtx_);
      if (!entries_.emplace(key, val).second)
        return;
    }
    // attach functor without lock
    log::core::debug("added actor: key = {}", key);
    actor_registry* reg = this;
    val->get()->attach_functor([key, reg]() { reg->erase(key); });
  }

  strong_actor_ptr get_impl(const std::string& key) const override {
    shared_guard guard{named_entries_mtx_};
    auto i = named_entries_.find(key);
    if (i == named_entries_.end())
      return nullptr;
    return i->second;
  }

  void put_impl(const std::string& key, strong_actor_ptr val) override {
    if (val == nullptr) {
      erase(key);
      return;
    }
    exclusive_guard guard{named_entries_mtx_};
    named_entries_.emplace(std::move(key), std::move(val));
  }

  using entries = std::unordered_map<actor_id, strong_actor_ptr>;

  mutable std::shared_mutex instances_mtx_;
  entries entries_;

  name_map named_entries_;
  mutable std::shared_mutex named_entries_mtx_;
};

class actor_clock_impl : public caf::actor_clock {
public:
  explicit actor_clock_impl(caf::actor_system& sys) {
    worker_ = sys.launch_thread("caf.clock", caf::thread_owner::system,
                                [this] { run(); });
  }

  ~actor_clock_impl() override {
    {
      std::unique_lock guard{mutex_};
      push({time_point::min(), caf::action{}});
    }
    cv_.notify_one();
    worker_.join();
  }

  caf::disposable schedule(time_point timeout, caf::action callback) override {
    if (!callback) {
      return {};
    }
    // Only wake up the dispatcher if the new timeout is smaller than the
    // current timeout.
    auto do_wakeup = false;
    {
      std::unique_lock guard{mutex_};
      do_wakeup = queue_.empty() || timeout < queue_.front().timeout;
      push({timeout, callback});
    }
    if (do_wakeup) {
      cv_.notify_one();
    }
    return std::move(callback).as_disposable();
  }

private:
  struct entry {
    time_point timeout;
    caf::action callback;
  };

  static constexpr auto entry_cmp = [](const entry& lhs, const entry& rhs) {
    // We want the smallest entry to be at the front of the queue. Since
    // std::push_heap will put the largest element at the front, we need to
    // invert the comparison.
    return lhs.timeout > rhs.timeout;
  };

  void push(entry e) {
    queue_.push_back(std::move(e));
    std::push_heap(queue_.begin(), queue_.end(), entry_cmp);
  }

  void pop() {
    std::pop_heap(queue_.begin(), queue_.end(), entry_cmp);
    queue_.pop_back();
  }

  void run() {
    std::unique_lock guard{mutex_};
    while (queue_.empty()) {
      cv_.wait(guard);
    }
    for (;;) {
      auto& job = queue_.front();
      if (!job.callback) {
        return;
      }
      auto timeout = job.timeout;
      if (now() >= timeout) {
        auto fn = std::move(job.callback);
        pop();
        guard.unlock();
        fn.run();
        guard.lock();
        while (queue_.empty()) {
          cv_.wait(guard);
        }
      } else {
        cv_.wait_until(guard, timeout);
      }
    }
  }

  std::thread worker_;
  std::mutex mutex_;
  std::condition_variable cv_;
  std::vector<entry> queue_;
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
      cfg(&cfg),
      private_threads(parent) {
    memset(&flags, 0xFF, sizeof(flags)); // All flags are ON by default.
    print_state = std::make_unique<print_state_impl>(cfg);
    meta_objects_guard = detail::global_meta_objects_guard();
    if (!meta_objects_guard)
      detail::critical("unable to obtain the global meta objects guard");
    for (auto& hook : cfg.thread_hooks())
      hook->init(*parent);
    // Cache some configuration parameters for faster lookups at runtime.
    using string_list = std::vector<std::string>;
    if (get_or(cfg, "caf.metrics.disable-running-actors", false)) {
      flags.collect_running_actors_metrics = false;
    }
    if (auto lst = get_as<string_list>(cfg,
                                       "caf.metrics.filters.actors.includes")) {
      metrics_actors_includes = std::move(*lst);
    }
    if (auto lst = get_as<string_list>(cfg,
                                       "caf.metrics.filters.actors.excludes")) {
      metrics_actors_excludes = std::move(*lst);
    }
    // Spin up modules.
    for (auto fn : cfg.module_factories()) {
      auto mod_ptr = fn(*parent);
      auto mod_id = mod_ptr->id();
      modules[mod_id].reset(mod_ptr);
    }
    // Let there be daemons.
    modules[actor_system_module::daemons].reset(new detail::daemons(*parent));
    // Make sure meta objects are loaded.
    auto gmos = detail::global_meta_objects();
    if (gmos.size() < id_block::core_module::end
        || gmos[id_block::core_module::begin].type_name.empty()) {
      detail::critical("actor_system created without calling "
                       "caf::init_global_meta_objects<>() before");
    }
    if (modules[actor_system_module::middleman] != nullptr) {
      if (gmos.size() < detail::io_module_end
          || gmos[detail::io_module_begin].type_name.empty()) {
        detail::critical("I/O module loaded without calling "
                         "caf::io::middleman::init_global_meta_objects()");
      }
    }
    // Allow the callback to override any configuration parameter and to
    // initialize member variables before we set the defaults.
    if (custom_setup != nullptr) {
      custom_setup(*parent, cfg, custom_setup_data);
    }
    // Initialize the logger before any other module.
    if (!logger) {
      logger = detail::asynchronous_logger::make(*parent);
      CAF_SET_LOGGER_SYS(parent);
    }
    // Make sure we have a clock.
    if (!clock) {
      clock = std::make_unique<actor_clock_impl>(*parent);
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
    // Initialize the state for each module and give each module the opportunity
    // to adapt the system configuration.
    for (auto& mod : modules)
      if (mod)
        mod->init(cfg);
    // Start all modules.
    registry.start();
    private_threads.start();
    for (auto& mod : modules)
      if (mod)
        mod->start();
    logger->start();
  }

  ~impl() {
    {
      auto lg = log::core::trace("");
      log::core::debug("shutdown actor system");
      if (flags.await_actors_before_shutdown) {
        await_running_actors_count_equal(0);
      }
      // stop modules in reverse order
      for (auto i = modules.rbegin(); i != modules.rend(); ++i) {
        auto& ptr = *i;
        if (ptr != nullptr) {
          log::core::debug("stop module {}", ptr->name());
          ptr->stop();
        }
      }
      log::core::debug("stop scheduler");
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

  telemetry::actor_metrics make_actor_metrics(std::string_view name) {
    telemetry::actor_metrics result;
    if (flags.collect_running_actors_metrics) {
      result.running_count
        = base_metrics.running_count->get_or_add({{"name", name}});
    }
    auto matches = [name](const std::string& glob) {
      return detail::match_wildcard_pattern(name, glob);
    };
    auto enable_optional_metrics
      = std::ranges::any_of(metrics_actors_includes, matches)
        && std::ranges::none_of(metrics_actors_excludes, matches);
    if (enable_optional_metrics) {
      result.processed_messages
        = base_metrics.processed_messages->get_or_add({{"name", name}});
      result.processing_time
        = base_metrics.processing_time->get_or_add({{"name", name}});
      result.mailbox_time
        = base_metrics.mailbox_time->get_or_add({{"name", name}});
      result.mailbox_size
        = base_metrics.mailbox_size->get_or_add({{"name", name}});
    }
    return result;
  }

  size_t inc_running_actors_count(actor_id who) {
    auto count = ++running_actors_count;
    log::system::debug("actor {} increased running count to {}", who, count);
    return count;
  }

  size_t dec_running_actors_count(actor_id who) {
    auto count = --running_actors_count;
    log::system::debug("actor {} decreased running count to {}", who, count);
    if (count <= 1) {
      std::unique_lock guard{running_actors_mtx};
      running_actors_cv.notify_all();
    }
    return count;
  }

  void await_running_actors_count_equal(size_t expected,
                                        timespan timeout = infinite) {
    CAF_ASSERT(expected == 0 || expected == 1);
    auto lg = log::core::trace("expected = {}", expected);
    std::unique_lock guard{running_actors_mtx};
    auto pred = [this, &expected] {
      auto running = running_actors_count.load();
      log::core::debug("running = {}, expected = {}", running, expected);
      return running == expected;
    };
    if (timeout == infinite)
      running_actors_cv.wait(guard, pred);
    else
      running_actors_cv.wait_for(guard, timeout, pred);
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
  actor_registry_impl registry;

  /// The number of currently running actors.
  std::atomic<size_t> running_actors_count = 0;

  /// Mutex for the running actors count condition variable.
  mutable std::mutex running_actors_mtx;

  /// Condition variable for waiting on the running actors count.
  mutable std::condition_variable running_actors_cv;

  /// Manages log output.
  intrusive_ptr<detail::asynchronous_logger> logger;

  /// Stores the system-wide clock.
  std::unique_ptr<actor_clock> clock;

  /// Stores the actor system scheduler.
  std::unique_ptr<caf::scheduler> scheduler;

  /// Stores optional actor system components.
  module_array modules;

  // Bundles various flags for the actor system into a single, memory-efficient
  // structure.
  struct flags_t {
    /// Stores whether the system should wait for running actors on shutdown.
    bool await_actors_before_shutdown : 1;

    /// Stores whether the system should keep track of how many actors are
    /// currently running per actor type.
    bool collect_running_actors_metrics : 1;
  };

  /// Stores flags that affect the entire actor system.
  flags_t flags;

  /// The system-wide, user-provided configuration.
  actor_system_config* cfg;

  /// Caches the configuration parameter `caf.metrics.filters.actors.includes`
  /// for faster lookups at runtime.
  std::vector<std::string> metrics_actors_includes;

  /// Caches the configuration parameter `caf.metrics.filters.actors.excludes`
  /// for faster lookups at runtime.
  std::vector<std::string> metrics_actors_excludes;

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
    detail::panic("CAF ABI token mismatch: got {}, expected {}",
                  static_cast<int>(token), CAF_VERSION_MAJOR);
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

detail::global_meta_objects_guard_type
actor_system::meta_objects_guard() const noexcept {
  return impl_->meta_objects_guard;
}

telemetry::actor_metrics
actor_system::make_actor_metrics(std::string_view name) {
  return impl_->make_actor_metrics(name);
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
  return impl_->flags.await_actors_before_shutdown;
}

void actor_system::await_actors_before_shutdown(bool new_value) {
  impl_->flags.await_actors_before_shutdown = new_value;
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

void actor_system::await_all_actors_done() const {
  await_running_actors_count_equal(0);
}

size_t actor_system::inc_running_actors_count(actor_id who) {
  return impl_->inc_running_actors_count(who);
}

size_t actor_system::dec_running_actors_count(actor_id who) {
  return impl_->dec_running_actors_count(who);
}

size_t actor_system::running_actors_count() const {
  return impl_->running_actors_count.load();
}

void actor_system::await_running_actors_count_equal(size_t expected,
                                                    timespan timeout) const {
  impl_->await_running_actors_count_equal(expected, timeout);
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
  return intrusive_ptr<actor_companion>{actor_cast<actor_companion*>(hdl),
                                        add_ref};
}

void actor_system::thread_started(thread_owner owner) {
  for (auto& hook : impl_->cfg->thread_hooks())
    hook->thread_started(owner);
}

void actor_system::thread_terminates() {
  for (auto& hook : impl_->cfg->thread_hooks())
    hook->thread_terminates();
}

std::pair<event_based_actor*, actor_launcher>
actor_system::spawn_inactive_impl(spawn_options options) {
  using actor_type = event_based_actor;
  CAF_SET_LOGGER_SYS(this);
  actor_config cfg{&scheduler(), nullptr};
  cfg.flags = abstract_actor::is_inactive_flag;
  if (has_detach_flag(options))
    cfg.flags |= abstract_actor::is_detached_flag;
  if (has_hide_flag(options))
    cfg.flags |= abstract_actor::is_hidden_flag;
  cfg.mbox_factory = mailbox_factory();
  auto res = make_actor<actor_type>(next_actor_id(), node(), this, cfg);
  auto* ptr = actor_cast<actor_type*>(res);
  return {ptr, actor_launcher{actor_cast<strong_actor_ptr>(std::move(res)),
                              &scheduler(), options}};
}

expected<strong_actor_ptr>
actor_system::dyn_spawn_impl(const std::string& name, message& args,
                             caf::scheduler* sched, bool check_interface,
                             const mpi* expected_ifs) {
  using result_type = expected<strong_actor_ptr>;
  auto lg = log::core::trace(
    "name = {}, args = {}, check_interface = {}, expected_ifs = {}", name, args,
    check_interface, expected_ifs);
  if (name.empty())
    return result_type{unexpect, sec::invalid_argument};
  auto* fs = impl_->cfg->get_actor_factory(name);
  if (fs == nullptr)
    return result_type{unexpect, sec::unknown_type};
  actor_config cfg{sched != nullptr ? sched : &scheduler()};
  auto res = (*fs)(*this, cfg, args);
  if (!res.first)
    return result_type{unexpect, sec::cannot_spawn_actor_from_arguments};
  if (check_interface && !assignable(res.second, *expected_ifs))
    return result_type{unexpect, sec::unexpected_actor_messaging_interface};
  return std::move(res.first);
}

detail::private_thread* actor_system::acquire_private_thread() {
  return impl_->private_threads.acquire();
}

void actor_system::release_private_thread(detail::private_thread* ptr) {
  impl_->private_threads.release(ptr);
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

void actor_system::do_launch(local_actor* ptr, caf::scheduler* ctx,
                             spawn_options options) {
  if (!has_hide_flag(options)) {
    ptr->setf(abstract_actor::is_registered_flag);
    inc_running_actors_count(ptr->id());
    // Note: decrementing the count happens in abstract_actor::cleanup().
  }
  ptr->launch(ctx, has_lazy_init_flag(options));
}

// -- callbacks for actor_system_access ----------------------------------------

void actor_system::set_logger(intrusive_ptr<detail::asynchronous_logger> ptr) {
  impl_->logger = std::move(ptr);
  CAF_SET_LOGGER_SYS(this);
}

void actor_system::set_clock(std::unique_ptr<actor_clock> ptr) {
  impl_->clock = std::move(ptr);
}

void actor_system::set_scheduler(std::unique_ptr<caf::scheduler> ptr) {
  impl_->scheduler = std::move(ptr);
}

void actor_system::set_node(node_id id) {
  impl_->node = id;
}

void actor_system::message_rejected(abstract_actor*) {
  impl_->base_metrics.rejected_messages->inc();
}

} // namespace caf

namespace caf::detail {

detail::daemons* actor_system_access::daemons() {
  auto ptr = sys_->impl_->modules[actor_system_module::daemons].get();
  return static_cast<detail::daemons*>(ptr);
}

} // namespace caf::detail
