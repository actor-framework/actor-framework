// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/logger.hpp"

#include "caf/actor_proxy.hpp"
#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/config.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/atomic_ref_counted.hpp"
#include "caf/detail/get_process_id.hpp"
#include "caf/detail/meta_object.hpp"
#include "caf/detail/pretty_type_name.hpp"
#include "caf/detail/set_thread_name.hpp"
#include "caf/detail/sync_ring_buffer.hpp"
#include "caf/local_actor.hpp"
#include "caf/make_counted.hpp"
#include "caf/message.hpp"
#include "caf/string_algorithms.hpp"
#include "caf/term.hpp"
#include "caf/thread_owner.hpp"
#include "caf/timestamp.hpp"

#include <algorithm>
#include <condition_variable>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <thread>
#include <unordered_map>
#include <utility>

namespace caf {

namespace {

// Stores the ID of the currently running actor.
thread_local actor_id current_actor_id;

// Stores a pointer to the system-wide logger.
thread_local intrusive_ptr<logger> current_logger_ptr;

struct print_adapter {
  std::ostream& out;
  void push_back(char c) {
    out.put(c);
  }
  int end() {
    return 0;
  }
  template <class Iterator, class Sentinel>
  void insert(int, Iterator iter, Sentinel sentinel) {
    while (iter != sentinel)
      out.put(*iter++);
  }
};

constexpr std::string_view log_level_name[] = {
  "QUIET", "",     "", "ERROR", "",      "", "WARN", "",
  "",      "INFO", "", "",      "DEBUG", "", "",     "TRACE",
};

/// Converts a verbosity level to its integer counterpart.
unsigned to_level_int(std::string_view x) {
  if (x == "error")
    return CAF_LOG_LEVEL_ERROR;
  if (x == "warning")
    return CAF_LOG_LEVEL_WARNING;
  if (x == "info")
    return CAF_LOG_LEVEL_INFO;
  if (x == "debug")
    return CAF_LOG_LEVEL_DEBUG;
  if (x == "trace")
    return CAF_LOG_LEVEL_TRACE;
  return CAF_LOG_LEVEL_QUIET;
}

// Default logger implementation.
class default_logger : public logger, public detail::atomic_ref_counted {
public:
  // -- constants --------------------------------------------------------------

  /// Configures the size of the circular event queue.
  static constexpr size_t queue_size = 128;

  // -- member types -----------------------------------------------------------

  enum field_type {
    invalid_field,
    category_field,
    class_name_field,
    date_field,
    file_field,
    line_field,
    message_field,
    method_field,
    newline_field,
    priority_field,
    runtime_field,
    thread_field,
    actor_field,
    percent_sign_field,
    plain_text_field,
  };

  /// Encapsulates a single logging event.
  struct event {
    /// Level/priority of the event.
    unsigned level;

    /// Current line in the file.
    unsigned line_number;

    /// Name of the category (component) logging the event.
    std::string_view component;

    /// Name of the current file.
    const char* file_name;

    /// Name of the current function as reported by `__func__`.
    const char* function_name;

    /// User-provided message.
    std::string message;

    /// Thread ID of the caller.
    std::thread::id tid;

    /// Actor ID of the caller.
    actor_id aid;

    /// Timestamp of the event.
    timestamp tstamp;

    event(unsigned lvl, unsigned ln, std::string_view cat, const char* fn,
          const char* fun, std::string&& msg, std::thread::id t,
          actor_id a = invalid_actor_id, timestamp ts = make_timestamp())
      : level(lvl),
        line_number(ln),
        component(cat),
        file_name(fn),
        function_name(fun),
        message(std::move(msg)),
        tid(t),
        aid(a),
        tstamp(ts) {
      // nop
    }
  };

  /// Combines various logging-related flags and parameters into a bitfield.
  struct config {
    /// Stores `max(file_verbosity, console_verbosity)`.
    unsigned verbosity : 4;

    /// Configures the verbosity for file output.
    unsigned file_verbosity : 4;

    /// Configures the verbosity for console output.
    unsigned console_verbosity : 4;

    /// Configures whether the logger immediately writes its output in the
    /// calling thread, bypassing its queue. Use this option only in
    /// single-threaded test environments.
    bool inline_output : 1;

    /// Configures whether the logger generates colored output.
    bool console_coloring : 1;

    config()
      : verbosity(CAF_LOG_LEVEL),
        file_verbosity(CAF_LOG_LEVEL),
        console_verbosity(CAF_LOG_LEVEL),
        inline_output(false),
        console_coloring(false) {
      // nop
    }
  };

  /// Represents a single format string field.
  struct field {
    field_type kind;
    std::string text;
  };

  /// Stores a parsed format string as list of fields.
  using line_format = std::vector<field>;

  // -- constructors, destructors, and assignment operators --------------------

  default_logger(actor_system& sys) : t0_(make_timestamp()), system_(sys) {
    // nop
  }

  // -- logging ----------------------------------------------------------------

  /// Writes an entry to the event-queue of the logger.
  /// @thread-safe
  void do_log(const context& ctx, std::string&& msg) override {
    auto ev
      = std::make_unique<event>(ctx.level, ctx.line_number, ctx.component,
                                ctx.file_name, ctx.function_name,
                                std::move(msg), std::this_thread::get_id(),
                                logger::thread_local_aid(), make_timestamp());
    if (cfg_.inline_output)
      handle_event(*ev);
    else
      queue_.push(std::move(ev));
  }

  // -- properties -------------------------------------------------------------
  /// Returns whether the logger is configured to accept input for given
  /// component and log level.
  bool accepts(unsigned level, std::string_view component_name) override {
    if (level > cfg_.verbosity)
      return false;
    return std::none_of(global_filter_.begin(), global_filter_.end(),
                        [=](std::string_view name) {
                          return name == component_name;
                        });
  }

  /// Returns the output format used for the log file.
  const line_format& file_format() const {
    return file_format_;
  }

  /// Returns the output format used for the console.
  const line_format& console_format() const {
    return console_format_;
  }

  unsigned verbosity() const noexcept {
    return cfg_.verbosity;
  }

  unsigned file_verbosity() const noexcept {
    return cfg_.file_verbosity;
  }

  unsigned console_verbosity() const noexcept {
    return cfg_.console_verbosity;
  }

  // -- static utility functions -----------------------------------------------

  /// Renders the date of `x` in ISO 8601 format.
  static void render_date(std::ostream& out, timestamp x) {
    print_adapter adapter{out};
    detail::print(adapter, x);
  }

  /// Parses `format_str` into a format description vector.
  /// @warning The returned vector can have pointers into `format_str`.
  static line_format parse_format(const std::string& format_str) {
    std::vector<field> res;
    auto plain_text_first = format_str.begin();
    bool read_percent_sign = false;
    auto i = format_str.begin();
    for (; i != format_str.end(); ++i) {
      if (read_percent_sign) {
        field_type ft;
        switch (*i) { // clang-format off
          case 'c': ft = category_field;     break;
          case 'C': ft = class_name_field;   break;
          case 'd': ft = date_field;         break;
          case 'F': ft = file_field;         break;
          case 'L': ft = line_field;         break;
          case 'm': ft = message_field;      break;
          case 'M': ft = method_field;       break;
          case 'n': ft = newline_field;      break;
          case 'p': ft = priority_field;     break;
          case 'r': ft = runtime_field;      break;
          case 't': ft = thread_field;       break;
          case 'a': ft = actor_field;        break;
          case '%': ft = percent_sign_field; break;
          default: // clang-format on
            ft = invalid_field;
            std::cerr << "invalid field specifier in format string: " << *i
                      << std::endl;
        }
        if (ft != invalid_field)
          res.emplace_back(field{ft, std::string{}});
        plain_text_first = i + 1;
        read_percent_sign = false;
      } else {
        if (*i == '%') {
          if (plain_text_first != i)
            res.emplace_back(
              field{plain_text_field, std::string{plain_text_first, i}});
          read_percent_sign = true;
        }
      }
    }
    if (plain_text_first != i)
      res.emplace_back(
        field{plain_text_field, std::string{plain_text_first, i}});
    return res;
  }

  /// Renders `x` using the line format `lf` to `out`.
  void render(std::ostream& out, const line_format& lf, const event& x) const {
    auto ms_time_diff = [](timestamp t0, timestamp tn) {
      using namespace std::chrono;
      return duration_cast<milliseconds>(tn - t0).count();
    };
    // clang-format off
    for (auto& f : lf)
      switch (f.kind) {
        case category_field:     out << x.component;                 break;
        case class_name_field:   out << "null";                      break;
        case date_field:         render_date(out, x.tstamp);         break;
        case file_field:         out << x.file_name;                 break;
        case line_field:         out << x.line_number;               break;
        case message_field:      out << x.message;                   break;
        case method_field:       out << x.function_name;             break;
        case newline_field:      out << std::endl;                   break;
        case priority_field:     out << log_level_name[x.level];     break;
        case runtime_field:      out << ms_time_diff(t0_, x.tstamp); break;
        case thread_field:       out << x.tid;                       break;
        case actor_field:        out << "actor" << x.aid;            break;
        case percent_sign_field: out << '%';                         break;
        case plain_text_field:   out << f.text;                      break;
        default: ; // nop
      }
    // clang-format on
  }

  // -- reference counting -----------------------------------------------------

  /// Increases the reference count of the coordinated.
  void ref_logger() const noexcept final {
    this->ref();
  }

  /// Decreases the reference count of the coordinated and destroys the object
  /// if necessary.
  void deref_logger() const noexcept final {
    this->deref();
  }

  // -- initialization ---------------------------------------------------------

  void init(const actor_system_config& cfg) override {
    CAF_IGNORE_UNUSED(cfg);
    namespace lg = defaults::logger;
    using std::string;
    using string_list = std::vector<std::string>;
    auto get_verbosity = [&cfg](std::string_view key) -> unsigned {
      if (auto str = get_if<string>(&cfg, key))
        return to_level_int(*str);
      return CAF_LOG_LEVEL_QUIET;
    };
    auto read_filter = [&cfg](string_list& var, std::string_view key) {
      if (auto lst = get_as<string_list>(cfg, key))
        var = std::move(*lst);
    };
    cfg_.inline_output = get_or(cfg, "caf.scheduler.policy", "") == "testing";
    cfg_.file_verbosity = get_verbosity("caf.logger.file.verbosity");
    cfg_.console_verbosity = get_verbosity("caf.logger.console.verbosity");
    cfg_.verbosity = std::max(cfg_.file_verbosity, cfg_.console_verbosity);
    if (cfg_.verbosity == CAF_LOG_LEVEL_QUIET)
      return;
    if (cfg_.file_verbosity > CAF_LOG_LEVEL_QUIET
        && cfg_.console_verbosity > CAF_LOG_LEVEL_QUIET) {
      read_filter(file_filter_, "caf.logger.file.excluded-components");
      read_filter(console_filter_, "caf.logger.console.excluded-components");
      std::sort(file_filter_.begin(), file_filter_.end());
      std::sort(console_filter_.begin(), console_filter_.end());
      std::set_intersection(file_filter_.begin(), file_filter_.end(),
                            console_filter_.begin(), console_filter_.end(),
                            std::back_inserter(global_filter_));
    } else if (cfg_.file_verbosity > CAF_LOG_LEVEL_QUIET) {
      read_filter(file_filter_, "caf.logger.file.excluded-components");
      global_filter_ = file_filter_;
    } else {
      read_filter(console_filter_, "caf.logger.console.excluded-components");
      global_filter_ = console_filter_;
    }
    // Parse the format string.
    file_format_
      = parse_format(get_or(cfg, "caf.logger.file.format", lg::file::format));
    console_format_ = parse_format(
      get_or(cfg, "caf.logger.console.format", lg::console::format));
    // If not set to `false`, CAF enables colored output when writing to TTYs.
    cfg_.console_coloring = get_or(cfg, "caf.logger.console.colored", true);
  }

  bool open_file() {
    if (file_verbosity() == CAF_LOG_LEVEL_QUIET || file_name_.empty())
      return false;
    file_.open(file_name_, std::ios::out | std::ios::app);
    if (!file_) {
      std::cerr << "unable to open log file " << file_name_ << std::endl;
      return false;
    }
    return true;
  }

  // -- event handling ---------------------------------------------------------

  void handle_event(const event& x) {
    handle_file_event(x);
    handle_console_event(x);
  }

  void handle_file_event(const event& x) {
    // Print to file if available.
    if (file_ && x.level <= file_verbosity()
        && none_of(file_filter_.begin(), file_filter_.end(),
                   [&x](std::string_view name) { return name == x.component; }))
      render(file_, file_format_, x);
  }

  void handle_console_event(const event& x) {
    if (x.level > console_verbosity())
      return;
    if (std::any_of(console_filter_.begin(), console_filter_.end(),
                    [&x](std::string_view name) {
                      return name == x.component;
                    }))
      return;
    if (cfg_.console_coloring) {
      switch (x.level) {
        default:
          break;
        case CAF_LOG_LEVEL_ERROR:
          std::clog << term::red;
          break;
        case CAF_LOG_LEVEL_WARNING:
          std::clog << term::yellow;
          break;
        case CAF_LOG_LEVEL_INFO:
          std::clog << term::green;
          break;
        case CAF_LOG_LEVEL_DEBUG:
          std::clog << term::cyan;
          break;
        case CAF_LOG_LEVEL_TRACE:
          std::clog << term::blue;
          break;
      }
      render(std::clog, console_format_, x);
      std::clog << term::reset_endl;
    } else {
      render(std::clog, console_format_, x);
      std::clog << std::endl;
    }
  }

  void log_first_line() {
    if (!accepts(CAF_LOG_LEVEL_DEBUG, "caf"))
      return;
    auto make_message = [&](int level, const auto& filter) {
      auto lvl_str = log_level_name[level];
      std::string msg = "verbosity = ";
      msg.insert(msg.end(), lvl_str.begin(), lvl_str.end());
      msg += ", node = ";
      msg += to_string(system_.node());
      msg += ", excluded-components = ";
      detail::stringification_inspector f{msg};
      detail::save(f, filter);
      return msg;
    };
    namespace lg = defaults::logger;
    auto loc = detail::source_location::current();
    event ev{CAF_LOG_LEVEL_DEBUG,
             static_cast<unsigned>(loc.line()),
             "caf.logger",
             loc.file_name(),
             loc.function_name(),
             "",
             std::this_thread::get_id(),
             0,
             make_timestamp()};
    ev.message = make_message(cfg_.file_verbosity, file_filter_);
    handle_file_event(ev);
    ev.message = make_message(cfg_.console_verbosity, console_filter_);
    handle_console_event(ev);
  }

  void log_last_line() {
    if (!accepts(CAF_LOG_LEVEL_DEBUG, "caf"))
      return;
    auto loc = detail::source_location::current();
    event ev{CAF_LOG_LEVEL_DEBUG,
             static_cast<unsigned>(loc.line()),
             "caf.logger",
             loc.file_name(),
             loc.function_name(),
             "bye",
             std::this_thread::get_id(),
             0,
             make_timestamp()};
    handle_event(ev);
  }

  // -- thread management ------------------------------------------------------

  void run() {
    // Bail out without printing anything if the first event we receive is the
    // shutdown (empty) event.
    if (auto first = queue_.pop(); first == nullptr) {
      return;
    } else {
      if (!open_file() && console_verbosity() == CAF_LOG_LEVEL_QUIET)
        return;
      log_first_line();
      handle_event(*first);
    }
    // Loop until receiving an empty message.
    for (;;) {
      if (auto next = queue_.pop()) {
        handle_event(*next);
      } else {
        log_last_line();
        return;
      }
    }
  }

  void start() override {
    parent_thread_ = std::this_thread::get_id();
    if (verbosity() == CAF_LOG_LEVEL_QUIET)
      return;
    file_name_ = get_or(system_.config(), "caf.logger.file.path",
                        defaults::logger::file::path);
    if (file_name_.empty()) {
      // No need to continue if console and log file are disabled.
      if (console_verbosity() == CAF_LOG_LEVEL_QUIET)
        return;
    } else {
      // Replace placeholders.
      const char pid[] = "[PID]";
      auto i = std::search(file_name_.begin(), file_name_.end(),
                           std::begin(pid), std::end(pid) - 1);
      if (i != file_name_.end()) {
        auto id = std::to_string(detail::get_process_id());
        file_name_.replace(i, i + sizeof(pid) - 1, id);
      }
      const char ts[] = "[TIMESTAMP]";
      i = std::search(file_name_.begin(), file_name_.end(), std::begin(ts),
                      std::end(ts) - 1);
      if (i != file_name_.end()) {
        auto t0_str = timestamp_to_string(t0_);
        file_name_.replace(i, i + sizeof(ts) - 1, t0_str);
      }
      const char node[] = "[NODE]";
      i = std::search(file_name_.begin(), file_name_.end(), std::begin(node),
                      std::end(node) - 1);
      if (i != file_name_.end()) {
        auto nid = to_string(system_.node());
        file_name_.replace(i, i + sizeof(node) - 1, nid);
      }
    }
    if (cfg_.inline_output) {
      // Open file immediately for inline output.
      open_file();
      log_first_line();
    } else {
      // Note: we don't call system_->launch_thread here since we don't want to
      //       set a logger context in the logger thread.
      auto f = [this](auto guard) {
        CAF_IGNORE_UNUSED(guard);
        detail::set_thread_name("caf.logger");
        system_.thread_started(thread_owner::system);
        run();
        system_.thread_terminates();
      };
      thread_ = std::thread{f, detail::global_meta_objects_guard()};
    }
  }

  void stop() override {
    if (cfg_.inline_output) {
      log_last_line();
      return;
    }
    if (!thread_.joinable())
      return;
    // Send an empty message to the logger thread to make it terminate.
    queue_.push(std::unique_ptr<event>{});
    thread_.join();
  }

  // -- member variables -------------------------------------------------------

  // Configures verbosity and output generation.
  config cfg_;

  // Filters events by component name before enqueuing a log event. Intersection
  // of file_filter_ and console_filter_ if both outputs are enabled.
  std::vector<std::string> global_filter_;

  // Filters events by component name for file output.
  std::vector<std::string> file_filter_;

  // Filters events by component name for console output.
  std::vector<std::string> console_filter_;

  // Identifies the thread that called `logger::start`.
  std::thread::id parent_thread_;

  // Format for generating file output.
  line_format file_format_;

  // Format for generating console output.
  line_format console_format_;

  // Stream for file output.
  std::fstream file_;

  // Filled with log events by other threads.
  detail::sync_ring_buffer<std::unique_ptr<event>, queue_size> queue_;

  // Stores the assembled name of the log file.
  std::string file_name_;

  // Executes `logger::run`.
  std::thread thread_;

  // Timestamp of the first log event.
  timestamp t0_;

  // References the parent system.
  actor_system& system_;
};

} // namespace

logger::line_builder::line_builder() {
  // nop
}

logger::line_builder&&
logger::line_builder::operator<<(const local_actor* self) && {
  return std::move(*this) << self->name();
}

logger::line_builder&&
logger::line_builder::operator<<(const std::string& str) && {
  return std::move(*this) << str.c_str();
}

logger::line_builder&&
logger::line_builder::operator<<(std::string_view str) && {
  if (!str_.empty() && str_.back() != ' ')
    str_ += " ";
  str_.insert(str_.end(), str.begin(), str.end());
  return std::move(*this);
}

logger::line_builder&& logger::line_builder::operator<<(const char* str) && {
  if (!str_.empty() && str_.back() != ' ')
    str_ += " ";
  str_ += str;
  return std::move(*this);
}

logger::line_builder&& logger::line_builder::operator<<(char x) && {
  const char buf[] = {x, '\0'};
  return std::move(*this) << buf;
}

void logger::legacy_api_log(unsigned level, std::string_view component,
                            std::string msg, detail::source_location loc) {
  context ctx{level, component, loc.line(), loc.file_name(),
              loc.function_name()};
  do_log(ctx, std::move(msg));
}

actor_id logger::thread_local_aid() {
  return current_actor_id;
}

actor_id logger::thread_local_aid(actor_id aid) {
  std::swap(current_actor_id, aid);
  return aid;
}

intrusive_ptr<logger> logger::make(actor_system& sys) {
  return make_counted<default_logger>(sys);
}

void logger::set_current_actor_system(actor_system* x) {
  current_logger_ptr.reset(x != nullptr ? &x->logger() : nullptr);
}

logger* logger::current_logger() {
  return current_logger_ptr.get();
}

} // namespace caf
