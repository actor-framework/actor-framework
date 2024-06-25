// A simple key-value store that reads JSON commands from TCP connections.

#include "caf/net/middleman.hpp"
#include "caf/net/octet_stream/with.hpp"

#include "caf/actor_from_state.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/caf_main.hpp"
#include "caf/default_enum_inspect.hpp"
#include "caf/error.hpp"
#include "caf/flow/byte.hpp"
#include "caf/flow/string.hpp"
#include "caf/json_reader.hpp"
#include "caf/result.hpp"
#include "caf/scheduled_actor/flow.hpp"
#include "caf/sec.hpp"

#include <csignal>
#include <cstdint>
#include <string>

using namespace std::literals;

struct command;

CAF_BEGIN_TYPE_ID_BLOCK(key_value_store, first_custom_type_id)

  CAF_ADD_TYPE_ID(key_value_store, (command))

CAF_END_TYPE_ID_BLOCK(key_value_store)

// The command type for the key-value store. The store supports three commands:
// get, put, and del. The associated free functions allow CAF to serialize and
// deserialize this type.
enum class command_type : uint8_t {
  get,
  put,
  del,
};

std::string to_string(command_type value) {
  switch (value) {
    case command_type::get:
      return "get";
    case command_type::put:
      return "put";
    case command_type::del:
      return "del";
    default:
      return "invalid";
  }
}

bool from_string(std::string_view input, command_type& value) {
  if (input == "get") {
    value = command_type::get;
    return true;
  }
  if (input == "put") {
    value = command_type::put;
    return true;
  }
  if (input == "del") {
    value = command_type::del;
    return true;
  }
  return false;
}

bool from_integer(std::underlying_type_t<command_type> input,
                  command_type& value) {
  if (input > 2) {
    return false;
  }
  value = static_cast<command_type>(input);
  return true;
}

template <class Inspector>
bool inspect(Inspector& f, command_type& x) {
  return caf::default_enum_inspect(f, x);
}

// The command for the key-value store with inspect overload that allows us to
// read and write commands to and from the network (JSON-encoded).
struct command {
  command_type type;
  std::string key;
  std::optional<std::string> value;
};

template <class Inspector>
bool inspect(Inspector& f, command& x) {
  return f.object(x).fields(f.field("type", x.type), f.field("key", x.key),
                            f.field("value", x.value));
}

// -- constants ----------------------------------------------------------------

// Configures the port for the server to listen on.
static constexpr uint16_t default_port = 7788;

// Configures the maximum number of concurrent connections.
static constexpr size_t default_max_connections = 128;

// Configures the maximum number of buffered messages per connection.
static constexpr size_t max_outstanding_messages = 10;

// -- configuration setup ------------------------------------------------------

struct config : caf::actor_system_config {
  config() {
    opt_group{custom_options_, "global"} //
      .add<uint16_t>("port,p", "port to listen for incoming connections")
      .add<size_t>("max-connections,m", "limit for concurrent clients");
    opt_group{custom_options_, "tls"} //
      .add<std::string>("key-file,k", "path to the private key file")
      .add<std::string>("cert-file,c", "path to the certificate file");
  }

  caf::settings dump_content() const override {
    auto result = actor_system_config::dump_content();
    caf::put_missing(result, "port", default_port);
    caf::put_missing(result, "max-connections", default_max_connections);
    return result;
  }
};

// -- our key-value store actor ------------------------------------------------

struct kvs_state {
  std::unordered_map<std::string, std::string> store;

  // Retrieves the value for `key` or returns an error if `key` does not exist.
  caf::result<std::string> get(const std::string& key) {
    auto i = store.find(key);
    if (i != store.end()) {
      return i->second;
    }
    return make_error(caf::sec::no_such_key);
  }

  // Sets the value for `key` to `value` and returns the previous value.
  std::string put(const std::string& key, std::string value) {
    auto& entry = store[key];
    entry.swap(value);
    return value;
  }

  // Removes the value for `key` and returns the previous value.
  caf::result<std::string> del(const std::string& key) {
    auto i = store.find(key);
    if (i != store.end()) {
      auto result = std::move(i->second);
      store.erase(i);
      return result;
    }
    return make_error(caf::sec::no_such_key);
  }

  caf::behavior make_behavior() {
    return {
      [this](command cmd) -> caf::result<std::string> {
        switch (cmd.type) {
          case command_type::get:
            return get(cmd.key);
          case command_type::put:
            if (cmd.value)
              return put(cmd.key, std::move(*cmd.value));
            return caf::make_error(caf::sec::runtime_error, "invalid command");
          case command_type::del:
            return del(cmd.key);
          default:
            return caf::make_error(caf::sec::runtime_error, "invalid command");
        };
      },
    };
  }
};

// -- main ---------------------------------------------------------------------

namespace {

std::atomic<bool> shutdown_flag;

void set_shutdown_flag(int) {
  shutdown_flag = true;
}

} // namespace

int caf_main(caf::actor_system& sys, const config& cfg) {
  namespace ssl = caf::net::ssl;
  // Do a regular shutdown for CTRL+C and SIGTERM.
  auto default_term = signal(SIGTERM, set_shutdown_flag);
  auto default_sint = signal(SIGINT, set_shutdown_flag);
  // Read the configuration.
  auto port = caf::get_or(cfg, "port", default_port);
  auto pem = ssl::format::pem;
  auto key_file = caf::get_as<std::string>(cfg, "tls.key-file");
  auto cert_file = caf::get_as<std::string>(cfg, "tls.cert-file");
  auto max_connections = caf::get_or(cfg, "max-connections",
                                     default_max_connections);
  if (!key_file != !cert_file) {
    sys.println("*** inconsistent TLS config: declare neither file or both");
    return EXIT_FAILURE;
  }
  // Open up a TCP port for incoming connections and start the server.
  auto kvs = sys.spawn(caf::actor_from_state<kvs_state>);
  auto server
    = caf::net::octet_stream::with(sys)
        // Optionally enable TLS.
        .context(ssl::context::enable(key_file && cert_file)
                   .and_then(ssl::emplace_server(ssl::tls::v1_2))
                   .and_then(ssl::use_private_key_file(key_file, pem))
                   .and_then(ssl::use_certificate_file(cert_file, pem)))
        // Bind to the user-defined port.
        .accept(port)
        // Stop if the key-value store actor terminates.
        .monitor(kvs)
        // Limit how many clients may be connected at any given time.
        .max_connections(max_connections)
        // When started, run our worker actor to handle incoming connections.
        .start([&sys, kvs](caf::net::acceptor_resource<std::byte> events) {
          sys.spawn([events, kvs](caf::event_based_actor* self) {
            // For each buffer pair, we create a new flow ...
            events.observe_on(self).for_each([self, kvs](auto ev) {
              auto [pull, push] = ev.data();
              pull //
                .observe_on(self)
                // ... that converts the lines to commands ...
                .transform(caf::flow::byte::split_as_utf8_at('\n'))
                .map([](const caf::cow_string& line) {
                  caf::json_reader reader;
                  if (!reader.load(line.str()))
                    return std::shared_ptr<command>{}; // Invalid JSON.
                  auto ptr = std::make_shared<command>();
                  if (!reader.apply(*ptr))
                    return std::shared_ptr<command>{}; // Not a command.
                  return ptr;
                })
                .concat_map([self, kvs](std::shared_ptr<command> ptr) {
                  // If the `map` step failed, inject an error message.
                  if (!ptr) {
                    auto str = std::string{R"_({"error":"invalid command"})_"};
                    return self->make_observable()
                      .just(caf::cow_string{std::move(str)})
                      .as_observable();
                  }
                  // Send the command to the key-value store actor. On
                  // error, we return an error message to the client.
                  return self->mail(*ptr)
                    .request(kvs, 1s)
                    .as_observable<std::string>()
                    .map([](const std::string& str) {
                      return caf::cow_string{str};
                    })
                    .on_error_return([](const caf::error& what) {
                      auto str = std::string{R"_({"error":")_"};
                      str += to_string(what);
                      str += R"_("})_";
                      auto res = caf::cow_string{std::move(str)};
                      return caf::expected<caf::cow_string>{std::move(res)};
                    })
                    .as_observable();
                })
                // ... disconnects if the client is too slow ...
                .on_backpressure_buffer(max_outstanding_messages)
                // ... and pushes the results back to the client as bytes.
                .transform(caf::flow::string::to_chars("\n"))
                .map([](char ch) { return static_cast<std::byte>(ch); })
                .subscribe(push);
            });
          });
        });
  // Report any error to the user.
  if (!server) {
    sys.println("*** unable to run at port {}: {}", port, server.error());
    return EXIT_FAILURE;
  }
  // Wait for CTRL+C or SIGTERM.
  sys.println("*** server is running, press CTRL+C to stop");
  while (!shutdown_flag)
    std::this_thread::sleep_for(250ms);
  // Restore the default handlers.
  signal(SIGTERM, default_term);
  signal(SIGINT, default_sint);
  // Shut down the server.
  sys.println("*** shutting down");
  anon_send_exit(kvs, caf::exit_reason::user_shutdown);
  return EXIT_SUCCESS;
}

CAF_MAIN(caf::id_block::key_value_store, caf::net::middleman)
