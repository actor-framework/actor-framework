// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/lp/framing.hpp"

#include "caf/test/scenario.hpp"

#include "caf/net/fwd.hpp"
#include "caf/net/lp/with.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/octet_stream/transport.hpp"
#include "caf/net/socket_id.hpp"
#include "caf/net/stream_socket.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/async/promise.hpp"
#include "caf/detail/network_order.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/raise_error.hpp"
#include "caf/scheduled_actor/flow.hpp"
#include "caf/scoped_actor.hpp"

using namespace caf;
using namespace std::literals;

namespace {

using string_list = std::vector<std::string>;

class buffer {
public:
  void append(std::string str) {
    std::scoped_lock guard{mtx_};
    entries_.emplace_back(std::move(str));
    cv_.notify_all();
  }

  void set_error(error err) {
    std::scoped_lock guard{mtx_};
    err_ = std::move(err);
    cv_.notify_all();
  }

  std::pair<string_list, error> get() {
    std::scoped_lock guard{mtx_};
    return {entries_, err_};
  }

  template <class Duration>
  bool wait_for_entries(size_t num, Duration timeout) {
    std::unique_lock guard{mtx_};
    return cv_.wait_for(guard, timeout,
                        [this, num] { return entries_.size() >= num || err_; });
  }

private:
  std::mutex mtx_;
  std::condition_variable cv_;
  std::vector<std::string> entries_;
  error err_;
};

using buffer_ptr = std::shared_ptr<buffer>;

class app_t : public net::lp::upper_layer {
public:
  async::execution_context_ptr loop;

  net::lp::lower_layer* down = nullptr;

  buffer_ptr inputs;

  std::function<void(net::lp::lower_layer*)> cb;

  app_t(async::execution_context_ptr loop, buffer_ptr inputs)
    : loop(std::move(loop)), inputs(std::move(inputs)) {
    // nop
  }

  template <class Callback>
  static auto
  make(async::execution_context_ptr loop, Callback cb, buffer_ptr inputs) {
    auto ptr = std::make_unique<app_t>(std::move(loop), std::move(inputs));
    ptr->cb = std::move(cb);
    return ptr;
  }

  caf::error start(net::lp::lower_layer* down_ptr) override {
    down = down_ptr;
    down->request_messages();
    cb(down_ptr);
    cb = nullptr;
    return none;
  }

  void prepare_send() override {
    // nop
  }

  bool done_sending() override {
    return true;
  }

  void abort(const error&) override {
    // nop
  }

  void continue_reading() {
    loop->schedule_fn([this] { down->request_messages(); });
  }

  ptrdiff_t consume(byte_span buf) override {
    auto printable = [](std::byte x) {
      return ::isprint(static_cast<uint8_t>(x));
    };
    if (std::all_of(buf.begin(), buf.end(), printable)) {
      auto str_buf = reinterpret_cast<char*>(buf.data());
      inputs->append(std::string{str_buf, buf.size()});
      return static_cast<ptrdiff_t>(buf.size());
    } else {
      inputs->set_error(error{sec::invalid_argument, "non-printable"});
      return -1;
    }
  }
};

struct fixture {
  fixture() {
    mpx = net::multiplexer::make(nullptr);
    if (auto err = mpx->init()) {
      CAF_RAISE_ERROR("mpx->init failed");
    }
    mpx_thread = mpx->launch();
    auto fd_pair = net::make_stream_socket_pair();
    if (!fd_pair) {
      CAF_RAISE_ERROR("make_stream_socket_pair failed");
    }
    std::tie(fd1, fd2) = *fd_pair;
  }

  ~fixture() {
    mpx->shutdown();
    mpx_thread.join();
    if (fd1 != net::invalid_socket)
      net::close(fd1);
    if (fd2 != net::invalid_socket)
      net::close(fd2);
  }

  template <class Callback>
  void run_app(Callback cb, buffer_ptr buf) {
    auto app = app_t::make(mpx, std::move(cb), std::move(buf));
    auto client = net::lp::framing::make(std::move(app));
    auto transport = net::octet_stream::transport::make(fd2, std::move(client));
    auto mgr = net::socket_manager::make(mpx.get(), std::move(transport));
    mpx->start(mgr);
    fd2.id = net::invalid_socket_id;
  }

  net::multiplexer_ptr mpx;
  net::stream_socket fd1;
  net::stream_socket fd2;
  std::thread mpx_thread;
};

auto encode(std::string_view msg) {
  byte_buffer bytes;
  using detail::to_network_order;
  auto prefix = to_network_order(static_cast<uint32_t>(msg.size()));
  auto prefix_bytes = as_bytes(make_span(&prefix, 1));
  bytes.insert(bytes.end(), prefix_bytes.begin(), prefix_bytes.end());
  auto msg_bytes = as_bytes(make_span(msg));
  bytes.insert(bytes.end(), msg_bytes.begin(), msg_bytes.end());
  return bytes;
}

WITH_FIXTURE(fixture) {

SCENARIO("length-prefix framing reads data with 32-bit size headers") {
  GIVEN("a framing object with an app that consumes strings") {
    WHEN("pushing data into the unit-under-test") {
      auto buf = std::make_shared<buffer>();
      run_app([](net::lp::lower_layer*) {}, buf);
      net::write(fd1, encode("hello"sv));
      net::write(fd1, encode("world"sv));
      THEN("the app receives all strings as individual messages") {
        require(buf->wait_for_entries(2, 1s));
        auto [entries, err] = buf->get();
        if (err) {
          fail("unexpected error: {}", err);
        }
        if (check_eq(entries.size(), 2u)) {
          check_eq(entries[0], "hello");
          check_eq(entries[1], "world");
        }
      }
    }
  }
}

} // WITH_FIXTURE(fixture)

void run_writer(net::stream_socket fd) {
  net::multiplexer::block_sigpipe();
  std::ignore = allow_sigpipe(fd, false);
  auto guard = net::make_socket_guard(fd);
  std::vector<std::string_view> inputs{"first", "second", "pause", "third",
                                       "fourth"};
  byte_buffer rd_buf;
  rd_buf.resize(512);
  for (auto input : inputs) {
    write(fd, encode(input));
    read(fd, rd_buf);
  }
}

SCENARIO("lp::with(...).connect(...) translates between flows and socket I/O") {
  GIVEN("a connected socket with a writer at the other end") {
    auto maybe_sockets = net::make_stream_socket_pair();
    require(maybe_sockets.has_value());
    auto [fd1, fd2] = *maybe_sockets;
    auto writer = std::thread{run_writer, fd1};
    WHEN("calling length_prefix_framing::run") {
      THEN("actors can consume the resulting flow") {
        caf::actor_system_config cfg;
        cfg.set("caf.scheduler.max-threads", 2);
        cfg.set("caf.scheduler.policy", "sharing");
        cfg.load<net::middleman>();
        caf::actor_system sys{cfg};
        auto buf = std::make_shared<std::vector<std::string>>();
        caf::actor hdl;
        auto conn
          = net::lp::with(sys) //
              .connect(fd2)
              .start([&](auto pull, auto push) {
                hdl = sys.spawn([buf, pull, push](event_based_actor* self) {
                  pull.observe_on(self)
                    .do_on_next([buf](const net::lp::frame& x) {
                      std::string str;
                      for (auto val : x.bytes())
                        str.push_back(static_cast<char>(val));
                      buf->push_back(std::move(str));
                    })
                    .map([](const net::lp::frame& x) {
                      std::string response = "ok ";
                      for (auto val : x.bytes())
                        response.push_back(static_cast<char>(val));
                      return net::lp::frame{as_bytes(make_span(response))};
                    })
                    .subscribe(push);
                });
              });
        conn.or_else([this](const error& err) { //
          fail("connect failed: {}", err);
        });
        scoped_actor self{sys};
        self->wait_for(hdl);
        if (check_eq(buf->size(), 5u)) {
          check_eq(buf->at(0), "first");
          check_eq(buf->at(1), "second");
          check_eq(buf->at(2), "pause");
          check_eq(buf->at(3), "third");
          check_eq(buf->at(4), "fourth");
        }
      }
    }
    writer.join();
  }
}

} // namespace
