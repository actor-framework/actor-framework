// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/http/client.hpp"

#include "caf/test/outline.hpp"
#include "caf/test/scenario.hpp"

#include "caf/net/fwd.hpp"
#include "caf/net/http/method.hpp"
#include "caf/net/http/response_header.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/octet_stream/transport.hpp"
#include "caf/net/socket_id.hpp"
#include "caf/net/socket_manager.hpp"
#include "caf/net/stream_socket.hpp"

#include "caf/async/promise.hpp"
#include "caf/raise_error.hpp"

using namespace caf;
using namespace std::literals;

namespace {

struct response_t {
  net::http::response_header hdr;
  byte_buffer payload;

  std::string_view payload_as_str() const noexcept {
    return {reinterpret_cast<const char*>(payload.data()), payload.size()};
  }
};

class app_t : public net::http::upper_layer::client {
public:
  // -- member variables -------------------------------------------------------

  net::http::lower_layer* down = nullptr;

  bool should_fail = false;

  async::promise<response_t> response;

  std::function<void(net::http::client*)> cb;

  // -- factories --------------------------------------------------------------

  template <class Callback>
  static auto make(Callback cb, async::promise<response_t> res = {}) {
    auto ptr = std::make_unique<app_t>();
    ptr->cb = std::move(cb);
    ptr->response = std::move(res);
    return ptr;
  }

  // -- implementation of http::upper_layer ------------------------------------

  error start(net::http::lower_layer::client* down_ptr) override {
    down = down_ptr;
    down->request_messages();
    cb(static_cast<net::http::client*>(down_ptr));
    cb = nullptr;
    return none;
  }

  void abort(const error& err) override {
    if (response) {
      response.set_error(err);
    }
  }

  void prepare_send() override {
    // nop
  }

  bool done_sending() override {
    return true;
  }

  ptrdiff_t consume(const net::http::response_header& response_hdr,
                    const_byte_span body) override {
    if (response) {
      response_t res;
      res.hdr = response_hdr;
      res.payload.assign(body.begin(), body.end());
      response.set_value(res);
    }
    if (should_fail) {
      should_fail = false;
      return -1;
    }
    return static_cast<ptrdiff_t>(body.size());
  }
};

auto to_str(caf::byte_span buffer) {
  return std::string_view{reinterpret_cast<const char*>(buffer.data()),
                          buffer.size()};
}

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
  void run_client(Callback cb, async::promise<response_t> res = {}) {
    auto app = app_t::make(std::move(cb), std::move(res));
    auto client = net::http::client::make(std::move(app));
    auto transport = net::octet_stream::transport::make(fd2, std::move(client));
    auto mgr = net::socket_manager::make(mpx.get(), std::move(transport));
    mpx->start(mgr);
    fd2.id = net::invalid_socket_id;
  }

  template <class Callback>
  void run_failing_client(Callback cb, async::promise<response_t> res = {}) {
    auto app = app_t::make(std::move(cb), std::move(res));
    app->should_fail = true;
    auto client = net::http::client::make(std::move(app));
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

WITH_FIXTURE(fixture) {

SCENARIO("the client sends HTTP requests") {
  GIVEN("a single line HTTP request") {
    WHEN("the client sends the message") {
      run_client([](net::http::client* client) {
        client->begin_header(net::http::method::get, "/foo/bar/index.html"sv);
        client->end_header();
      });
      THEN("the output contains the formatted request") {
        auto want = "GET /foo/bar/index.html HTTP/1.1\r\n\r\n"sv;
        byte_buffer buf;
        buf.resize(want.size());
        auto res = net::read(fd1, buf);
        check_eq(res, static_cast<ptrdiff_t>(want.size()));
        check_eq(to_str(buf), want);
      }
    }
  }
  GIVEN("a multi-line HTTP request") {
    WHEN("the client sends the message") {
      run_client([](net::http::client* client) {
        client->begin_header(net::http::method::get, "/foo/bar/index.html"sv);
        client->add_header_field("Host", "localhost:8090");
        client->add_header_field("User-Agent", "AwesomeLib/1.0");
        client->add_header_field("Accept-Encoding", "chunked");
        client->end_header();
      });
      THEN("the output contains the formatted request") {
        std::string_view want = "GET /foo/bar/index.html HTTP/1.1\r\n"
                                "Host: localhost:8090\r\n"
                                "User-Agent: AwesomeLib/1.0\r\n"
                                "Accept-Encoding: chunked\r\n\r\n";
        byte_buffer buf;
        buf.resize(want.size());
        auto res = net::read(fd1, buf);
        check_eq(res, static_cast<ptrdiff_t>(want.size()));
        check_eq(to_str(buf), want);
      }
    }
  }
  GIVEN("a multi-line HTTP request with a payload") {
    WHEN("the client sends the message") {
      run_client([](net::http::client* client) {
        client->begin_header(net::http::method::post, "/foo/bar/index.html"sv);
        client->add_header_field("Content-Length", "13");
        client->add_header_field("Content-Type", "plain/text");
        client->end_header();
        auto body = "Hello, world!"sv;
        client->send_payload(as_bytes(make_span(body)));
      });
      THEN("the output contains the formatted request") {
        std::string_view want = "POST /foo/bar/index.html HTTP/1.1\r\n"
                                "Content-Length: 13\r\n"
                                "Content-Type: plain/text\r\n"
                                "\r\n"
                                "Hello, world!";
        byte_buffer buf;
        buf.resize(want.size());
        auto res = net::read(fd1, buf);
        check_eq(res, static_cast<ptrdiff_t>(want.size()));
        check_eq(to_str(buf), want);
      }
    }
  }
  GIVEN("a multi-line HTTP request with a chunked payload") {
    WHEN("the client sends the message") {
      run_client([](net::http::client* client) {
        client->begin_header(net::http::method::post, "/foo/bar/index.html"sv);
        client->add_header_field("Content-Type", "plain/text");
        client->add_header_field("Transfer-Encoding", "chunked");
        client->end_header();
        auto chunk1 = "Hello, world!"sv;
        auto chunk2 = "Developer Network"sv;
        client->send_chunk(as_bytes(make_span(chunk1)));
        client->send_chunk(as_bytes(make_span(chunk2)));
        client->send_end_of_chunks();
      });
      THEN("the output contains the formatted request") {
        std::string_view want = "POST /foo/bar/index.html HTTP/1.1\r\n"
                                "Content-Type: plain/text\r\n"
                                "Transfer-Encoding: chunked\r\n"
                                "\r\n"
                                "D\r\n"
                                "Hello, world!\r\n"
                                "11\r\n"
                                "Developer Network\r\n"
                                "0\r\n"
                                "\r\n";
        byte_buffer buf;
        buf.resize(want.size());
        auto res = net::read(fd1, buf);
        check_eq(res, static_cast<ptrdiff_t>(want.size()));
        check_eq(to_str(buf), want);
      }
    }
  }
}

OUTLINE("sending all available HTTP methods") {
  GIVEN("a http request with <method> method") {
    auto method_name = block_parameters<std::string>();
    WHEN("the client sends the <enum value> message") {
      auto method = block_parameters<net::http::method>();
      run_client([method](auto* client) {
        client->begin_header(method, "/foo/bar"sv);
        client->end_header();
      });
      THEN("the output contains the formatted request") {
        auto want = detail::format("{} /foo/bar HTTP/1.1\r\n\r\n", method_name);
        byte_buffer buf;
        buf.resize(want.size());
        auto res = net::read(fd1, buf);
        check_eq(res, static_cast<ptrdiff_t>(want.size()));
        check_eq(to_str(buf), want);
      }
    }
  }
  EXAMPLES = R"(
    | method   | enum value |
    | GET      | get        |
    | HEAD     | head       |
    | POST     | post       |
    | PUT      | put        |
    | DELETE   | del        |
    | CONNECT  | connect    |
    | OPTIONS  | options    |
    | TRACE    | trace      |
  )";
}

SCENARIO("the client parses HTTP response into header fields") {
  GIVEN("a single line HTTP response") {
    std::string_view response = "HTTP/1.1 200 OK\r\n\r\n";
    WHEN("receiving from an HTTP server") {
      auto res_promise = async::promise<response_t>{};
      run_client([](auto*) {}, res_promise);
      net::write(fd1, as_bytes(make_span(response)));
      THEN("the HTTP layer parses the data and calls the application layer") {
        auto maybe_res = res_promise.get_future().get(1s);
        require(maybe_res.has_value());
        auto& res = *maybe_res;
        check_eq(res.hdr.version(), "HTTP/1.1");
        check_eq(res.hdr.status(), 200u);
        check_eq(res.hdr.status_text(), "OK");
        check_eq(res.hdr.num_fields(), 0u);
        check(res.payload.empty());
      }
    }
  }
  GIVEN("a multi line HTTP response") {
    std::string_view response = "HTTP/1.1 200 OK\r\n"
                                "Content-Type: text/plain\r\n\r\n";
    WHEN("receiving from an HTTP server") {
      auto res_promise = async::promise<response_t>{};
      run_client([](auto*) {}, res_promise);
      net::write(fd1, as_bytes(make_span(response)));
      THEN("the HTTP layer parses the data and calls the application layer") {
        auto maybe_res = res_promise.get_future().get(1s);
        require(maybe_res.has_value());
        auto& res = *maybe_res;
        check_eq(res.hdr.version(), "HTTP/1.1");
        check_eq(res.hdr.status(), 200u);
        check_eq(res.hdr.status_text(), "OK");
        check_eq(res.hdr.num_fields(), 1u);
        check_eq(res.hdr.field("Content-Type"), "text/plain");
        check(res.payload.empty());
      }
    }
  }
  GIVEN("a multi line HTTP response with body") {
    std::string_view response = "HTTP/1.1 200 OK\r\n"
                                "Content-Length: 13\r\n"
                                "Content-Type: text/plain\r\n\r\n"
                                "Hello, world!";
    WHEN("receiving from an HTTP server") {
      auto res_promise = async::promise<response_t>{};
      run_client([](auto*) {}, res_promise);
      net::write(fd1, as_bytes(make_span(response)));
      THEN("the HTTP layer parses the data and calls the application layer") {
        auto maybe_res = res_promise.get_future().get(1s);
        require(maybe_res.has_value());
        auto& res = *maybe_res;
        check_eq(res.hdr.version(), "HTTP/1.1");
        check_eq(res.hdr.status(), 200u);
        check_eq(res.hdr.status_text(), "OK");
        check_eq(res.hdr.field("Content-Type"), "text/plain");
        check_eq(res.hdr.field("Content-Length"), "13");
        check_eq(res.payload_as_str(), "Hello, world!");
      }
    }
  }
  GIVEN("a HTTP response arriving line by line") {
    auto line1 = "HTTP/1.1 200 OK\r\n"sv;
    auto line2 = "Content-Length: 13\r\n"sv;
    auto line3 = "Content-Type: text/plain\r\n\r\n"sv;
    auto line4 = "Hello, world!"sv;
    WHEN("receiving from an HTTP server") {
      auto res_promise = async::promise<response_t>{};
      run_client([](auto*) {}, res_promise);
      net::write(fd1, as_bytes(make_span(line1)));
      std::this_thread::sleep_for(1ms);
      net::write(fd1, as_bytes(make_span(line2)));
      std::this_thread::sleep_for(1ms);
      net::write(fd1, as_bytes(make_span(line3)));
      std::this_thread::sleep_for(1ms);
      net::write(fd1, as_bytes(make_span(line4)));
      THEN("the HTTP layer parses the data and calls the application layer") {
        auto maybe_res = res_promise.get_future().get(1s);
        require(maybe_res.has_value());
        auto& res = *maybe_res;
        check_eq(res.hdr.version(), "HTTP/1.1");
        check_eq(res.hdr.status(), 200u);
        check_eq(res.hdr.status_text(), "OK");
        check_eq(res.hdr.field("Content-Type"), "text/plain");
        check_eq(res.hdr.field("Content-Length"), "13");
        check_eq(res.payload_as_str(), "Hello, world!");
      }
    }
  }
  GIVEN("a HTTP response containing more data than content length") {
    std::string_view response = "HTTP/1.1 200 OK\r\n"
                                "Content-Length: 13\r\n"
                                "Content-Type: text/plain\r\n\r\n"
                                "Hello, world!<secret>";
    WHEN("receiving from an HTTP server") {
      auto res_promise = async::promise<response_t>{};
      run_client([](auto*) {}, res_promise);
      net::write(fd1, as_bytes(make_span(response)));
      THEN("the HTTP layer discards the extra content") {
        auto maybe_res = res_promise.get_future().get(1s);
        require(maybe_res.has_value());
        auto& res = *maybe_res;
        check_eq(res.hdr.version(), "HTTP/1.1");
        check_eq(res.hdr.status(), 200u);
        check_eq(res.hdr.status_text(), "OK");
        check_eq(res.hdr.field("Content-Type"), "text/plain");
        check_eq(res.hdr.field("Content-Length"), "13");
        check_eq(res.payload_as_str(), "Hello, world!");
      }
    }
  }
}

SCENARIO("the client receives invalid HTTP responses") {
  GIVEN("a response with an invalid header field") {
    std::string_view response = "HTTP/1.1 200 OK\r\n"
                                "FooBar\r\n"
                                "\r\n";
    WHEN("receiving from an HTTP server") {
      auto res_promise = async::promise<response_t>{};
      run_client([](auto*) {}, res_promise);
      net::write(fd1, as_bytes(make_span(response)));
      THEN("the HTTP layer parses the data and calls abort") {
        auto maybe_res = res_promise.get_future().get(1s);
        check(!maybe_res);
      }
    }
  }
  GIVEN("a response that exceeds the configured maximum size") {
    std::string_view response = "HTTP/1.1 200 OK\r\n"
                                "Content-Type: text/plain\r\n\r\n";
    WHEN("receiving from an HTTP server") {
      auto res_promise = async::promise<response_t>{};
      run_client([](auto* client) { client->max_response_size(10); },
                 res_promise);
      net::write(fd1, as_bytes(make_span(response)));
      THEN("the HTTP layer refuses the input") {
        auto maybe_res = res_promise.get_future().get(1s);
        check(!maybe_res);
      }
    }
  }
  GIVEN("a response with a too big content length") {
    std::string_view response = "HTTP/1.1 200 OK\r\n"
                                "Content-Type: text/plain\r\n"
                                "Content-Length: 10000000\r\n\r\nHello, world";
    WHEN("receiving from an HTTP server") {
      auto res_promise = async::promise<response_t>{};
      run_client([](auto*) {}, res_promise);
      net::write(fd1, as_bytes(make_span(response)));
      THEN("the HTTP layer parses the data and calls abort") {
        auto maybe_res = res_promise.get_future().get(1s);
        check(!maybe_res);
      }
    }
  }
  // TODO: implement chunked transfer encoding
  GIVEN("a response using chunked encoding") {
    std::string_view response = "HTTP/1.1 200 OK\r\n"
                                "Content-Type: text/plain\r\n"
                                "Transfer-Encoding: chunked\r\n"
                                "\r\n";
    WHEN("receiving from an HTTP server") {
      auto res_promise = async::promise<response_t>{};
      run_client([](auto*) {}, res_promise);
      net::write(fd1, as_bytes(make_span(response)));
      THEN("the HTTP layer parses the data and calls abort") {
        auto maybe_res = res_promise.get_future().get(1s);
        check(!maybe_res);
      }
    }
  }
}

SCENARIO("apps can return errors to abort the HTTP layer") {
  GIVEN("an app that fails consuming the HTTP response") {
    std::string_view response = "HTTP/1.1 200 OK\r\n"
                                "FooBar\r\n"
                                "\r\n";
    WHEN("the app returns -1 for the response in receives") {
      auto res_promise = async::promise<response_t>{};
      run_failing_client([](auto*) {}, res_promise);
      net::write(fd1, as_bytes(make_span(response)));
      THEN("the client calls abort") {
        auto maybe_res = res_promise.get_future().get(1s);
        check(!maybe_res);
      }
    }
  }
}

} // WITH_FIXTURE(fixture)

} // namespace
