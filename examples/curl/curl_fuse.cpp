/******************************************************************************\
 * This example                                                               *
 * - emulates a client launching a request every 10-300ms                     *
 * - uses a CURL-backend consisting of a master and 10 workers                *
 * - runs until it is shut down by a CTRL+C signal                            *
 *                                                                            *
 *                                                                            *
 * Schematic view:                                                            *
 *                                                                            *
 *    client      |    client_job    |    curl_master    |    curl_worker     *
 *          /--------------|*|-------------\       /-------------|*|          *
 *         /---------------|*|--------------\     /                           *
 *        /----------------|*|---------------\   /                            *
 *     |*| ----------------|*|----------------|*|----------------|*|          *
 *        \________________|*|_______________/   \                            *
 *         \_______________|*|______________/     \                           *
 *          \______________|*|_____________/       \-------------|*|          *
 *                                                                            *
 *                                                                            *
 * Communication pattern:                                                     *
 *                                                                            *
 *        client_job      curl_master        curl_worker                      *
 *          |                  |                  |                           *
 *          | ----(read)-----> |                  |                           *
 *          |                  | --(forward)----> |                           *
 *          |                                     |---\                       *
 *          |                                     |   |                       *
 *          |                                     |<--/                       *
 *          | <-------------(reply)-------------- |                           *
 *          X                                                                 *
\ ******************************************************************************/

// C includes
#include <time.h>
#include <signal.h>
#include <stdlib.h>

// C++ includes
#include <string>
#include <vector>
#include <random>
#include <iostream>

// libcurl
#include <curl/curl.h>

// libcaf
#include "caf/all.hpp"

// disable some clang warnings here caused by CURL
#ifdef __clang__
# pragma clang diagnostic ignored "-Wshorten-64-to-32"
# pragma clang diagnostic ignored "-Wdisabled-macro-expansion"
# pragma clang diagnostic ignored "-Wunused-const-variable"
#endif // __clang__

using namespace caf;

using buffer_type = std::vector<char>;

using read_atom = atom_constant<atom("read")>;
using fail_atom = atom_constant<atom("fail")>;
using next_atom = atom_constant<atom("next")>;
using reply_atom = atom_constant<atom("reply")>;
using finished_atom = atom_constant<atom("finished")>;

namespace color {

// UNIX terminal color codes
constexpr char reset[]        = "\033[0m";
constexpr char reset_endl[]   = "\033[0m\n";
constexpr char black[]        = "\033[30m";
constexpr char red[]          = "\033[31m";
constexpr char green[]        = "\033[32m";
constexpr char yellow[]       = "\033[33m";
constexpr char blue[]         = "\033[34m";
constexpr char magenta[]      = "\033[35m";
constexpr char cyan[]         = "\033[36m";
constexpr char white[]        = "\033[37m";
constexpr char bold_black[]   = "\033[1m\033[30m";
constexpr char bold_red[]     = "\033[1m\033[31m";
constexpr char bold_green[]   = "\033[1m\033[32m";
constexpr char bold_yellow[]  = "\033[1m\033[33m";
constexpr char bold_blue[]    = "\033[1m\033[34m";
constexpr char bold_magenta[] = "\033[1m\033[35m";
constexpr char bold_cyan[]    = "\033[1m\033[36m";
constexpr char bold_white[]   = "\033[1m\033[37m";

} // namespace color

// number of HTTP workers
constexpr size_t num_curl_workers = 10;

// minimum delay between HTTP requests
constexpr int min_req_interval = 10;

// maximum delay between HTTP requests
constexpr int max_req_interval = 300;

// provides print utility and each base_actor has a parent
class base_actor : public event_based_actor {
protected:
  base_actor(actor parent, std::string name, std::string color_code)
      : parent_(std::move(parent)),
        name_(std::move(name)),
        color_(std::move(color_code)),
        out_(this) {
    // nop
  }

  ~base_actor();

  inline actor_ostream& print() {
    return out_ << color_ << name_ << " (id = " << id() << "): ";
  }

  void on_exit() {
    print() << "on_exit" << color::reset_endl;
  }

  actor parent_;

private:
  std::string   name_;
  std::string   color_;
  actor_ostream out_;
};

base_actor::~base_actor() {
  // avoid weak-vtables warning
}

// encapsulates an HTTP request
class client_job : public base_actor {
public:
  explicit client_job(actor parent)
      : base_actor(std::move(parent), "client_job", color::blue) {
    // nop
  }

  ~client_job();

protected:
  behavior make_behavior() override {
    print() << "init" << color::reset_endl;
    send(parent_, read_atom::value, "http://www.example.com/index.html",
         uint64_t{0}, uint64_t{4095});
    return {
      [=](reply_atom, const buffer_type& buf) {
        print() << "successfully received " << buf.size() << " bytes"
                << color::reset_endl;
        quit();
      },
      [=](fail_atom) {
        print() << "failure" << color::reset_endl;
        quit();
      }
    };
  }
};

client_job::~client_job() {
  // avoid weak-vtables warning
}

// spawns HTTP requests
class client : public base_actor {
public:

  explicit client(const actor& parent)
      : base_actor(parent, "client", color::green),
        count_(0),
        re_(rd_()),
        dist_(min_req_interval, max_req_interval) {
    // nop
  }

  ~client();

protected:
  behavior make_behavior() override {
    using std::chrono::milliseconds;
    link_to(parent_);
    print() << "init" << color::reset_endl;
    // start 'loop'
    send(this, next_atom::value);
    return (
      [=](next_atom) {
        print() << "spawn new client_job (nr. "
            << ++count_
            << ")"
            << color::reset_endl;
        // client_job will use IO
        // and should thus be spawned in a separate thread
        spawn<client_job, detached+linked>(parent_);
        // compute random delay until next job is launched
        auto delay = dist_(re_);
        delayed_send(this, milliseconds(delay), next_atom::value);
      }
    );
  }

private:
  size_t count_;
  std::random_device rd_;
  std::default_random_engine re_;
  std::uniform_int_distribution<int> dist_;
};

client::~client() {
  // avoid weak-vtables warning
}

// manages a CURL session
class curl_worker : public base_actor {
public:
  explicit curl_worker(const actor& parent)
      : base_actor(parent, "curl_worker", color::yellow),
        curl_(nullptr) {
    // nop
  }

  ~curl_worker();

protected:
  behavior make_behavior() override {
    print() << "init" << color::reset_endl;
    curl_ = curl_easy_init();
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, &curl_worker::cb);
    curl_easy_setopt(curl_, CURLOPT_NOSIGNAL, 1);
    return (
      [=](read_atom, const std::string& fname, uint64_t offset, uint64_t range)
      -> message {
        print() << "read" << color::reset_endl;
        for (;;) {
          buf_.clear();
          // set URL
          curl_easy_setopt(curl_, CURLOPT_URL, fname.c_str());
          // set range
          std::ostringstream oss;
          oss << offset << "-" << range;
          curl_easy_setopt(curl_, CURLOPT_RANGE, oss.str().c_str());
          // set curl callback
          curl_easy_setopt(curl_, CURLOPT_WRITEDATA,
                           reinterpret_cast<void*>(this));
          // launch file transfer
          auto res = curl_easy_perform(curl_);
          if (res != CURLE_OK) {
            print() << "curl_easy_perform() failed: "
                    << curl_easy_strerror(res)
                    << color::reset_endl;
          } else {
            long hc = 0; // http return code
            curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &hc);
            switch (hc) {
              default:
                print() << "http error: download failed with "
                        << "'HTTP RETURN CODE': "
                        << hc
                        << color::reset_endl;
                break;
              case 200: // ok
              case 206: // partial content
                print() << "received "
                        << buf_.size()
                        << " bytes with 'HTTP RETURN CODE': "
                        << hc
                        << color::reset_endl;
                // tell parent that this worker is done
                send(parent_, finished_atom::value);
                return make_message(reply_atom::value, buf_);
              case 404: // file does not exist
                print() << "http error: download failed with "
                        << "'HTTP RETURN CODE': 404 (file does "
                        << "not exist!)"
                        << color::reset_endl;
            }
          }
          // avoid 100% cpu utilization if remote side is not accessible
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
      }
    );
  }

  void on_exit() {
    curl_easy_cleanup(curl_);
    print() << "on_exit" << color::reset_endl;
  }

private:
  static size_t cb(void* data, size_t bsize, size_t nmemb, void* userp) {
    size_t size = bsize * nmemb;
    auto& buf = reinterpret_cast<curl_worker*>(userp)->buf_;
    auto first = reinterpret_cast<char*>(data);
    auto last = first + bsize;
    buf.insert(buf.end(), first, last);
    return size;
  }

  CURL*       curl_;
  buffer_type buf_;
};

curl_worker::~curl_worker() {
  // avoid weak-vtables warning
}

// manages {num_curl_workers} workers with a round-robin protocol
class curl_master : public base_actor {
public:
  curl_master() : base_actor(invalid_actor, "curl_master", color::magenta) {
    // nop
  }

  ~curl_master();

protected:
  behavior make_behavior() override {
    print() << "init" << color::reset_endl;
    // spawn workers
    for(size_t i = 0; i < num_curl_workers; ++i) {
      idle_worker_.push_back(spawn<curl_worker, detached+linked>(this));
    }
    auto worker_finished = [=] {
      auto sender = current_sender();
      auto last = busy_worker_.end();
      auto i = std::find(busy_worker_.begin(), last, sender);
      if (i == last) {
        return;
      }
      idle_worker_.push_back(*i);
      busy_worker_.erase(i);
      print() << "worker is done" << color::reset_endl;
    };
    print() << "spawned " << idle_worker_.size()
            << " worker(s)" << color::reset_endl;
    return {
      [=](read_atom, const std::string&, uint64_t, uint64_t) {
        print() << "received {'read'}" << color::reset_endl;
        // forward job to an idle worker
        actor worker = idle_worker_.back();
        idle_worker_.pop_back();
        busy_worker_.push_back(worker);
        forward_to(worker);
        print() << busy_worker_.size() << " active jobs" << color::reset_endl;
        if (idle_worker_.empty()) {
          // wait until at least one worker finished its job
          become (
            keep_behavior,
            [=](finished_atom) {
              worker_finished();
              unbecome();
            }
          );
        }
      },
      [=](finished_atom) {
        worker_finished();
      }
    };
  }

private:
  std::vector<actor> idle_worker_;
  std::vector<actor> busy_worker_;
};

curl_master::~curl_master() {
  // avoid weak-vtables warning
}

// signal handling for ctrl+c
namespace {
std::atomic<bool> shutdown_flag{false};
} // namespace <anonymous>

int main() {
  // random number setup
  // install signal handler
  struct sigaction act;
  act.sa_handler = [](int) { shutdown_flag = true; };
  auto set_sighandler = [&] {
    if (sigaction(SIGINT, &act, 0)) {
      std::cerr << "fatal: cannot set signal handler" << std::endl;
      abort();
    }
  };
  set_sighandler();
  // initialize CURL
  curl_global_init(CURL_GLOBAL_DEFAULT);
  { // lifetime scope of self
    scoped_actor self;
    // spawn client and curl_master
    auto master = self->spawn<curl_master, detached>();
    self->spawn<client, detached>(master);
    // poll CTRL+C flag every second
    while (! shutdown_flag) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    aout(self) << color::cyan << "received CTRL+C" << color::reset_endl;
    // shutdown actors
    anon_send_exit(master, exit_reason::user_shutdown);
    // await actors
    act.sa_handler = [](int) { abort(); };
    set_sighandler();
    aout(self) << color::cyan
               << "await CURL; this may take a while "
                  "(press CTRL+C again to abort)"
               << color::reset_endl;
    self->await_all_other_actors_done();
  }
  // shutdown libcaf
  shutdown();
  // shutdown CURL
  curl_global_cleanup();
}
