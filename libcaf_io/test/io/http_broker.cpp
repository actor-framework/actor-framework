// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/config.hpp"

#define CAF_SUITE io_http_broker
#include "io-test.hpp"

#include <algorithm>
#include <cassert>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

using std::cerr;
using std::cout;
using std::endl;

using namespace caf;
using namespace caf::io;

namespace {

constexpr char http_valid_get[] = "GET / HTTP/1.1";

constexpr char http_get[] = "GET / HTTP/1.1\r\n"
                            "Host: localhost\r\n"
                            "Connection: close\r\n"
                            "Accept: text/plain\r\n"
                            "User-Agent: CAF/0.14\r\n"
                            "Accept-Language: en-US\r\n"
                            "\r\n";

constexpr char http_ok[] = "HTTP/1.1 200 OK\r\n"
                           "Content-Type: text/plain\r\n"
                           "Connection: close\r\n"
                           "Transfer-Encoding: chunked\r\n"
                           "\r\n"
                           "d\r\n"
                           "Hi there! :)\r\n"
                           "\r\n"
                           "0\r\n"
                           "\r\n"
                           "\r\n";

constexpr char http_error[] = "HTTP/1.1 404 Not Found\r\n"
                              "Connection: close\r\n"
                              "\r\n";

constexpr char newline[2] = {'\r', '\n'};

enum parser_state {
  receive_new_line,
  receive_continued_line,
  receive_second_newline_half
};

struct http_state {
  http_state(abstract_broker* self) : self_(self) {
    // nop
  }

  ~http_state() {
    aout(self_) << "http worker is destroyed";
  }

  std::vector<std::string> lines;
  parser_state ps = receive_new_line;
  abstract_broker* self_;
};

using http_broker = caf::stateful_actor<http_state, broker>;

behavior http_worker(http_broker* self, connection_handle hdl) {
  // tell network backend to receive any number of bytes between 1 and 1024
  self->configure_read(hdl, receive_policy::at_most(1024));
  return {
    [=](const new_data_msg& msg) {
      assert(!msg.buf.empty());
      assert(msg.handle == hdl);
      // interpret the received bytes as a string
      string_view msg_buf{reinterpret_cast<const char*>(msg.buf.data()),
                          msg.buf.size()};
      // extract lines from received buffer
      auto& lines = self->state.lines;
      auto i = msg_buf.begin();
      auto e = msg_buf.end();
      // search position of first newline in data chunk
      auto nl = std::search(i, e, std::begin(newline), std::end(newline));
      // store whether we are continuing a previously started line
      auto append_to_last_line = self->state.ps == receive_continued_line;
      // check whether our last chunk ended between \r and \n
      if (self->state.ps == receive_second_newline_half) {
        if (msg_buf.front() == '\n') {
          // simply skip this character
          ++i;
        }
      }
      // read line by line from our data chunk
      do {
        if (append_to_last_line) {
          append_to_last_line = false;
          auto& back = lines.back();
          back.insert(back.end(), i, nl);
        } else {
          lines.emplace_back(i, nl);
        }
        // if our last search didn't found a newline, we're done
        if (nl != e) {
          // skip newline and seek the next one
          i = nl + sizeof(newline);
          nl = std::search(i, e, std::begin(newline), std::end(newline));
        }
      } while (nl != e);
      // store current state of our parser
      if (msg_buf.back() == '\r') {
        self->state.ps = receive_second_newline_half;
        self->state.lines.pop_back(); // drop '\r' from our last read line
      } else if (msg_buf.back() == '\n') {
        self->state.ps = receive_new_line; // we've got a clean cut
      } else {
        self->state.ps = receive_continued_line; // interrupted in the
                                                 // middle
      }
      // we don't need to check for completion in any intermediate state
      if (self->state.ps != receive_new_line)
        return;
      // we have received the HTTP header if we have an empty line at the
      // end
      if (lines.size() > 1 && lines.back().empty()) {
        auto& out = self->wr_buf(hdl);
        auto append = [&](string_view str) {
          auto bytes = as_bytes(make_span(str));
          out.insert(out.end(), bytes.begin(), bytes.end());
        };
        // we only look at the first line in our example and reply with
        // our OK message if we receive exactly "GET / HTTP/1.1",
        // otherwise we send a 404 HTTP response
        if (lines.front() == http_valid_get)
          append(http_ok);
        else
          append(http_error);
        // write data and close connection
        self->flush(hdl);
        self->quit();
      }
    },
    [=](const connection_closed_msg&) { self->quit(); },
  };
}

behavior server(broker* self) {
  MESSAGE("server up and running");
  return {
    [=](const new_connection_msg& ncm) {
      MESSAGE("fork on new connection");
      self->fork(http_worker, ncm.handle);
    },
  };
}

class fixture {
public:
  using multiplexer_type = network::test_multiplexer;

  fixture() : system(cfg.load<io::middleman, network::test_multiplexer>()) {
    mpx_ = dynamic_cast<multiplexer_type*>(&system.middleman().backend());
    CAF_REQUIRE(mpx_ != nullptr);
    // spawn the actor-under-test
    aut_ = system.middleman().spawn_broker(server);
    // assign the acceptor handle to the AUT
    aut_ptr_ = static_cast<abstract_broker*>(actor_cast<abstract_actor*>(aut_));
    aut_ptr_->add_doorman(mpx_->new_doorman(acceptor_, 1u));
    // "open" a new connection to our server
    mpx_->add_pending_connect(acceptor_, connection_);
    mpx_->accept_connection(acceptor_);
  }

  ~fixture() {
    anon_send_exit(aut_, exit_reason::kill);
    // run the exit message and other pending messages explicitly,
    // since we do not invoke any "I/O" from this point on that would
    // trigger the exit message implicitly
    mpx_->flush_runnables();
  }

  // helper class for a nice-and-easy "mock(...).check_response(...)" syntax
  class mock_t {
  public:
    mock_t(fixture* thisptr) : this_(thisptr) {
      // nop
    }

    mock_t(const mock_t&) = default;

    mock_t& check_response(const std::string& x) {
      auto bytes = as_bytes(make_span(x));
      auto& buf = this_->mpx_->output_buffer(this_->connection_);
      REQUIRE_GE(buf.size(), x.size());
      REQUIRE(std::equal(buf.begin(),
                         buf.begin() + static_cast<ptrdiff_t>(x.size()),
                         bytes.begin()));
      buf.erase(buf.begin(), buf.begin() + static_cast<ptrdiff_t>(x.size()));
      return *this;
    }

    fixture* this_;
  };

  // mocks some input for our AUT and allows to
  // check the output for this operation
  mock_t mock(string_view what) {
    auto bytes = as_bytes(make_span(what));
    byte_buffer buf{bytes.begin(), bytes.end()};
    mpx_->virtual_send(connection_, std::move(buf));
    return {this};
  }

  actor_system_config cfg;
  actor_system system;
  actor aut_;
  abstract_broker* aut_ptr_;
  network::test_multiplexer* mpx_;
  accept_handle acceptor_ = accept_handle::from_int(1);
  connection_handle connection_ = connection_handle::from_int(1);
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

CAF_TEST(valid_response) {
  // write a GET message and check_response an OK message as result
  mock(http_get).check_response(http_ok);
}

CAF_TEST(invalid_response) {
  // write a GET with invalid path and check_response a 404 message as result
  mock("GET /kitten.gif HTTP/1.1\r\n\r\n").check_response(http_error);
}

END_FIXTURE_SCOPE()
