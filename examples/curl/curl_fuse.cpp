/******************************************************************************
 * This example                                                               *
 * - emulates a client launching a request every 10-300ms                     *
 * - uses a CURL-backend consisting of a coordinator and 10 workers           *
 * - runs until it is shut down by a CTRL+C signal                            *
 *                                                                            *
 *                                                                            *
 * Schematic view:                                                            *
 *                                                                            *
 *    client      |    client_job    |    coordinator    |     worker         *
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
 *        client_job       coordinator         worker                         *
 *          |                  |                  |                           *
 *          | ----(read)-----> |                  |                           *
 *          |                  | --(forward)----> |                           *
 *          |                                     |---\                       *
 *          |                                     |   |                       *
 *          |                                     |<--/                       *
 *          | <-------------(reply)-------------- |                           *
 *          X                                                                 *
 ******************************************************************************/

#include "caf/io/middleman.hpp"

#include "caf/actor_from_state.hpp"
#include "caf/actor_ostream.hpp"
#include "caf/actor_system.hpp"
#include "caf/caf_main.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/scoped_actor.hpp"

#include <csignal>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <random>
#include <string>
#include <vector>

CAF_PUSH_WARNINGS
#include <curl/curl.h>
CAF_POP_WARNINGS

// disable some clang warnings here caused by CURL macros
#ifdef __clang__
#  pragma clang diagnostic ignored "-Wshorten-64-to-32"
#  pragma clang diagnostic ignored "-Wdisabled-macro-expansion"
#  pragma clang diagnostic ignored "-Wunused-const-variable"
#endif // __clang__

CAF_BEGIN_TYPE_ID_BLOCK(curl_fuse, first_custom_type_id)

  CAF_ADD_TYPE_ID(curl_fuse, (std::vector<char>) )

  CAF_ADD_ATOM(curl_fuse, read_atom)
  CAF_ADD_ATOM(curl_fuse, fail_atom)
  CAF_ADD_ATOM(curl_fuse, next_atom)
  CAF_ADD_ATOM(curl_fuse, reply_atom)
  CAF_ADD_ATOM(curl_fuse, finished_atom)

CAF_END_TYPE_ID_BLOCK(curl_fuse)

using namespace caf;
using namespace std::literals;

using buffer_type = std::vector<char>;

namespace color {

// UNIX terminal color codes
constexpr char reset[] = "\033[0m";
constexpr char reset_endl[] = "\033[0m\n";
constexpr char black[] = "\033[30m";
constexpr char red[] = "\033[31m";
constexpr char green[] = "\033[32m";
constexpr char yellow[] = "\033[33m";
constexpr char blue[] = "\033[34m";
constexpr char magenta[] = "\033[35m";
constexpr char cyan[] = "\033[36m";
constexpr char white[] = "\033[37m";
constexpr char bold_black[] = "\033[1m\033[30m";
constexpr char bold_red[] = "\033[1m\033[31m";
constexpr char bold_green[] = "\033[1m\033[32m";
constexpr char bold_yellow[] = "\033[1m\033[33m";
constexpr char bold_blue[] = "\033[1m\033[34m";
constexpr char bold_magenta[] = "\033[1m\033[35m";
constexpr char bold_cyan[] = "\033[1m\033[36m";
constexpr char bold_white[] = "\033[1m\033[37m";

} // namespace color

// number of HTTP workers
constexpr size_t num_workers = 10;

// minimum delay between HTTP requests
constexpr int min_req_interval = 10;

// maximum delay between HTTP requests
constexpr int max_req_interval = 300;

// provides print utility and a name
struct base_state {
  base_state(event_based_actor* thisptr) : self(thisptr) {
    // nop
  }

  actor_ostream print() {
    return aout(self) << color << self->name() << " (id = " << self->id()
                      << "): ";
  }

  virtual bool init(std::string m_color) {
    color = std::move(m_color);
    print() << "started" << color::reset_endl;
    return true;
  }

  virtual ~base_state() {
    print() << "done" << color::reset_endl;
  }

  event_based_actor* self;
  std::string color;
};

// encapsulates an HTTP request
struct client_job_state : base_state {
  static inline const char* name = "curl.client-job";

  client_job_state(event_based_actor* self, actor parent_hdl)
    : base_state(self), parent(std::move(parent_hdl)) {
    // nop
  }

  behavior make_behavior() {
    if (!init(color::blue))
      return {}; // returning an empty behavior terminates the actor
    self
      ->mail(read_atom_v, "http://www.example.com/index.html", uint64_t{0},
             uint64_t{4095})
      .send(parent);
    return {
      [this](reply_atom, const buffer_type& buf) {
        print() << "successfully received " << buf.size() << " bytes"
                << color::reset_endl;
        self->quit();
      },
      [this](fail_atom) {
        print() << "failure" << color::reset_endl;
        self->quit();
      },
    };
  }

  actor parent;
};

// the client spawns HTTP requests
struct client_state : base_state {
  client_state(event_based_actor* selfptr, actor parent_hdl)
    : base_state(selfptr),
      parent(std::move(parent_hdl)),
      count(0),
      re(std::random_device{}()),
      dist(min_req_interval, max_req_interval) {
    // nop
  }

  behavior make_behavior() {
    self->link_to(parent);
    if (!init(color::green))
      return {}; // returning an empty behavior terminates the actor
    self->mail(next_atom_v).send(self);
    return {
      [this](next_atom) {
        print() << "spawn new client_job (nr. " << ++count << ")"
                << color::reset_endl;
        // client_job will do I/O and should be spawned in a separate thread
        self->spawn<detached + linked>(actor_from_state<client_job_state>,
                                       parent);
        // compute random delay until next job is launched
        auto delay = dist(re);
        self->mail(next_atom_v)
          .delay(std::chrono::milliseconds(delay))
          .send(self);
      },
    };
  }

  actor parent;
  size_t count;
  std::default_random_engine re;
  std::uniform_int_distribution<int> dist;
  static inline const char* name = "curl.client";
};

// manages a CURL session
struct worker_state : base_state {
  worker_state(event_based_actor* selfptr, actor parent_hdl)
    : base_state(selfptr), parent(std::move(parent_hdl)) {
    // nop
  }

  ~worker_state() override {
    if (curl != nullptr)
      curl_easy_cleanup(curl);
  }

  static size_t callback(void* data, size_t bsize, size_t nmemb, void* userp) {
    size_t size = bsize * nmemb;
    auto& buf = reinterpret_cast<worker_state*>(userp)->buf;
    auto first = reinterpret_cast<char*>(data);
    auto last = first + bsize;
    buf.insert(buf.end(), first, last);
    return size;
  }

  bool init(std::string m_color) override {
    curl = curl_easy_init();
    if (curl == nullptr)
      return false;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &worker_state::callback);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
    return base_state::init(std::move(m_color));
  }

  behavior make_behavior() {
    if (!init(color::yellow))
      return {}; // returning an empty behavior terminates the actor
    return {
      [=](read_atom, const std::string& fname, uint64_t offset,
          uint64_t range) -> message {
        print() << "read" << color::reset_endl;
        for (;;) {
          buf.clear();
          // set URL
          curl_easy_setopt(curl, CURLOPT_URL, fname.c_str());
          // set range
          std::ostringstream oss;
          oss << offset << "-" << range;
          curl_easy_setopt(curl, CURLOPT_RANGE, oss.str().c_str());
          // set curl callback
          curl_easy_setopt(curl, CURLOPT_WRITEDATA,
                           reinterpret_cast<void*>(this));
          // launch file transfer
          auto res = curl_easy_perform(curl);
          if (res != CURLE_OK) {
            print() << "curl_easy_perform() failed: " << curl_easy_strerror(res)
                    << color::reset_endl;
          } else {
            long hc = 0; // http return code
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &hc);
            switch (hc) {
              default:
                print() << "http error: download failed with "
                        << "'HTTP RETURN CODE': " << hc << color::reset_endl;
                break;
              case 200: // ok
              case 206: // partial content
                print() << "received " << buf.size()
                        << " bytes with 'HTTP RETURN CODE': " << hc
                        << color::reset_endl;
                // tell parent that this worker is done
                self->mail(finished_atom_v).send(parent);
                return make_message(reply_atom_v, std::move(buf));
              case 404: // file does not exist
                print() << "http error: download failed with "
                        << "'HTTP RETURN CODE': 404 (file does "
                        << "not exist!)" << color::reset_endl;
            }
          }
          // avoid 100% cpu utilization if remote side is not accessible
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
      },
    };
  }

  actor parent;
  CURL* curl = nullptr;
  buffer_type buf;
  static inline const char* name = "curl.worker";
};

struct coordinator_state : base_state {
  using base_state::base_state;

  behavior make_behavior() {
    if (!init(color::magenta))
      return {}; // returning an empty behavior terminates the actor
    // spawn workers
    for (size_t i = 0; i < num_workers; ++i) {
      auto fn = actor_from_state<worker_state>;
      idle.push_back(self->spawn<detached + linked>(fn, self));
    }
    print() << "spawned " << idle.size() << " worker(s)" << color::reset_endl;
    return {
      [this](read_atom rd, std::string& str, uint64_t x, uint64_t y) {
        print() << "received {'read'}" << color::reset_endl;
        // forward job to an idle worker
        actor worker = idle.back();
        idle.pop_back();
        busy.push_back(worker);
        self->delegate(worker, rd, std::move(str), x, y);
        print() << busy.size() << " active jobs" << color::reset_endl;
        if (idle.empty()) {
          // wait until at least one worker finished its job
          self->become(keep_behavior, [this](finished_atom) {
            finished();
            self->unbecome();
          });
        }
      },
      [this](finished_atom) { finished(); },
    };
  }

  void finished() {
    auto sender = self->current_sender();
    auto last = busy.end();
    auto i = std::find(busy.begin(), last, sender);
    if (i == last)
      return;
    idle.push_back(*i);
    busy.erase(i);
    print() << "worker is done" << color::reset_endl;
  }

  std::vector<actor> idle;
  std::vector<actor> busy;
  static inline const char* name = "curl.coordinator";
};

// signal handling for ctrl+c
std::atomic<bool> shutdown_flag;

void caf_main(actor_system& sys) {
  // install signal handler
  struct sigaction act;
  act.sa_handler = [](int) { shutdown_flag = true; };
  auto set_sighandler = [&] {
    if (sigaction(SIGINT, &act, nullptr) != 0) {
      std::cerr << "fatal: cannot set signal handler" << std::endl;
      abort();
    }
  };
  set_sighandler();
  // initialize CURL
  curl_global_init(CURL_GLOBAL_DEFAULT);
  // get a scoped actor for the communication with our CURL actors
  scoped_actor self{sys};
  // spawn client and coordinator
  auto coordinator = self->spawn<detached>(actor_from_state<coordinator_state>);
  self->spawn<detached>(actor_from_state<client_state>, coordinator);
  // poll CTRL+C flag every second
  while (!shutdown_flag)
    std::this_thread::sleep_for(std::chrono::seconds(1));
  aout(self) << color::cyan << "received CTRL+C" << color::reset_endl;
  // shutdown actors
  anon_send_exit(coordinator, exit_reason::user_shutdown);
  // await actors
  act.sa_handler = [](int) { abort(); };
  set_sighandler();
  aout(self) << color::cyan
             << "await CURL; this may take a while "
                "(press CTRL+C again to abort)"
             << color::reset_endl;
  self->await_all_other_actors_done();
  // shutdown CURL
  curl_global_cleanup();
}

CAF_MAIN(id_block::curl_fuse, io::middleman)
