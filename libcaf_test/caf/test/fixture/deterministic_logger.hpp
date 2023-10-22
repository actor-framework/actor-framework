#include "caf/logger.hpp"

using namespace caf;

namespace caf::test::fixture {

/// A logger implementation for deterministic fixture to log into console
class deterministic_logger : public caf::logger,
                             public caf::detail::atomic_ref_counted {
public:
  /// Increases the reference count of the coordinated.
  void ref_logger() const noexcept final {
    this->ref();
  }

  /// Decreases the reference count of the coordinated and destroys the object
  /// if necessary.
  void deref_logger() const noexcept final {
    this->deref();
  }

  // -- constructors, destructors, and assignment operators --------------------

  deterministic_logger(actor_system& sys) : system_(sys) {
    // nop
  }

  // -- logging ----------------------------------------------------------------

  /// Writes an entry to the event-queue of the logger.
  /// @thread-safe
  void do_log(const context& ctx, std::string&& msg) override;

  /// Returns whether the logger is configured to accept input for given
  /// component and log level.
  bool accepts(unsigned level, std::string_view) override;

  // -- initialization ---------------------------------------------------------

  /// Allows the logger to read its configuration from the actor system config.
  void init(const actor_system_config& cfg) override;

  // -- event handling ---------------------------------------------------------

  /// Starts any background threads needed by the logger.
  void start() override {
    // nop
  }

  /// Stops all background threads of the logger.
  void stop() override {
    // nop
  }

  // -- member variables -------------------------------------------------------

  // Stores logger verbosity to ignore log messages.
  uint32_t verbosity_;

  // References the parent system.
  actor_system& system_;
};

} // namespace caf::test::fixture
