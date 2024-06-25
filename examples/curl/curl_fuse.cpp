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
#include "caf/mail_cache.hpp"
#include "caf/scoped_actor.hpp"

#include <csignal>
#include <cstdlib>
#include <ctime>
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

using namespace std::literals;

using buffer_type = std::vector<char>;

// number of HTTP workers
constexpr size_t num_workers = 10;

// minimum delay between HTTP requests
constexpr int min_req_interval = 10;

// maximum delay between HTTP requests
constexpr int max_req_interval = 300;

// provides print utility and a name
struct base_state {
  explicit base_state(caf::event_based_actor* selfptr) : self(selfptr) {
    // nop
  }

  virtual bool init(caf::term new_color) {
    color = new_color;
    self->println(color, "{}[{}]: started", self->name(), self->id());
    return true;
  }

  virtual ~base_state() {
    self->println(color, "{}[{}]: done", self->name(), self->id());
  }

  caf::event_based_actor* self;
  caf::term color = caf::term::reset;
};

// encapsulates an HTTP request
struct client_job_state : base_state {
  static inline const char* name = "curl.client-job";

  client_job_state(caf::event_based_actor* self, caf::actor parent_hdl)
    : base_state(self), parent(std::move(parent_hdl)) {
    // nop
  }

  caf::behavior make_behavior() {
    if (!init(caf::term::blue))
      return {}; // returning an empty behavior terminates the actor
    self
      ->mail(read_atom_v, "http://www.example.com/index.html", uint64_t{0},
             uint64_t{4095})
      .send(parent);
    return {
      [this](reply_atom, const buffer_type& buf) {
        self->println(color, "{}[{}]: successfully received {} bytes",
                      self->name(), self->id(), buf.size());
        self->quit();
      },
      [this](fail_atom) {
        self->println(color, "{}[{}]: failure", self->name(), self->id());
        self->quit();
      },
    };
  }

  caf::actor parent;
};

// the client spawns HTTP requests
struct client_state : base_state {
  client_state(caf::event_based_actor* self, caf::actor parent)
    : base_state(self),
      parent(std::move(parent)),
      re(std::random_device{}()),
      dist(min_req_interval, max_req_interval) {
    // nop
  }

  caf::behavior make_behavior() {
    self->link_to(parent);
    if (!init(caf::term::green))
      return {}; // returning an empty behavior terminates the actor
    self->mail(next_atom_v).send(self);
    return {
      [this](next_atom) {
        self->println(color, "{}[{}]: spawn new client_job (nr. {})",
                      self->name(), self->id(), ++count);
        // client_job will do I/O and should be spawned in a separate thread
        self->spawn<caf::detached + caf::linked>(
          caf::actor_from_state<client_job_state>, parent);
        // compute random delay until next job is launched
        auto delay = dist(re);
        self->mail(next_atom_v)
          .delay(std::chrono::milliseconds(delay))
          .send(self);
      },
    };
  }

  caf::actor parent;
  size_t count = 0;
  std::default_random_engine re;
  std::uniform_int_distribution<int> dist;
  static inline const char* name = "curl.client";
};

// manages a CURL session
struct worker_state : base_state {
  worker_state(caf::event_based_actor* self, caf::actor parent)
    : base_state(self), parent(std::move(parent)) {
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

  bool init(caf::term new_color) override {
    curl = curl_easy_init();
    if (curl == nullptr)
      return false;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &worker_state::callback);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
    return base_state::init(new_color);
  }

  caf::behavior make_behavior() {
    if (!init(caf::term::yellow))
      return {}; // returning an empty behavior terminates the actor
    return {
      [=](read_atom, const std::string& fname, uint64_t offset,
          uint64_t range) -> caf::message {
        self->println(color, "{}[{}]: start reading {} at offset {}",
                      self->name(), self->id(), fname, offset);
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
            self->println(color, "{}[{}]: curl_easy_perform() failed: {}",
                          self->name(), self->id(), curl_easy_strerror(res));
          } else {
            long hc = 0; // http return code
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &hc);
            switch (hc) {
              default:
                self->println(color,
                              "{}[{}]: download failed with HTTP code {}",
                              self->name(), self->id(), hc);
                break;
              case 200: // ok
              case 206: // partial content
                self->println(color,
                              "{}[{}]: received {} bytes with HTTP code {}",
                              self->name(), self->id(), buf.size(), hc);
                // tell parent that this worker is done
                self->mail(finished_atom_v).send(parent);
                return caf::make_message(reply_atom_v, std::move(buf));
              case 404: // file does not exist
                self->println(color,
                              "{}[{}]: download failed with HTTP code 404 "
                              "(file does not exist)",
                              self->name(), self->id());
            }
          }
          // avoid 100% cpu utilization if remote side is not accessible
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
      },
    };
  }

  caf::actor parent;
  CURL* curl = nullptr;
  buffer_type buf;
  static inline const char* name = "curl.worker";
};

struct coordinator_state : base_state {
  explicit coordinator_state(caf::event_based_actor* self)
    : base_state(self), cache(self, 10) {
    // nop
  }

  caf::behavior make_behavior() {
    if (!init(caf::term::magenta))
      return {}; // returning an empty behavior terminates the actor
    // spawn workers
    for (size_t i = 0; i < num_workers; ++i) {
      auto fn = caf::actor_from_state<worker_state>;
      idle.push_back(self->spawn<caf::detached + caf::linked>(fn, self));
    }
    self->println(color, "{}[{}]: spawned {} worker(s)", self->name(),
                  self->id(), idle.size());
    return {
      [this](read_atom rd, std::string str, uint64_t x, uint64_t y) {
        self->println(color, "{}[{}]: received {{'read'}}", self->name(),
                      self->id());
        // forward job to an idle worker
        auto worker = idle.back();
        idle.pop_back();
        busy.push_back(worker);
        self->delegate(worker, rd, std::move(str), x, y);
        self->println(color, "{}[{}]: scheduled new work -> {} active jobs",
                      self->name(), self->id(), busy.size());
        if (idle.empty()) {
          // wait until at least one worker finished its job
          self->become(
            caf::keep_behavior,
            [this](finished_atom) {
              finished();
              self->unbecome();
              cache.unstash();
            },
            [this](caf::message msg) { cache.stash(std::move(msg)); });
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
    self->println(color, "{}[{}]: worker finished -> {} active jobs",
                  self->name(), self->id(), busy.size());
  }

  std::vector<caf::actor> idle;
  std::vector<caf::actor> busy;
  caf::mail_cache cache;
  static inline const char* name = "curl.coordinator";
};

// signal handling for ctrl+c
std::atomic<bool> shutdown_flag;

void caf_main(caf::actor_system& sys) {
  // install signal handler
  struct sigaction act;
  act.sa_handler = [](int) { shutdown_flag = true; };
  auto set_sighandler = [&] {
    if (sigaction(SIGINT, &act, nullptr) != 0) {
      sys.println("fatal: cannot set signal handler");
      abort();
    }
  };
  set_sighandler();
  // initialize CURL
  curl_global_init(CURL_GLOBAL_DEFAULT);
  // get a scoped actor for the communication with our CURL actors
  caf::scoped_actor self{sys};
  // spawn client and coordinator
  using caf::actor_from_state;
  using caf::detached;
  auto coordinator = self->spawn<detached>(actor_from_state<coordinator_state>);
  self->spawn<detached>(actor_from_state<client_state>, coordinator);
  // poll CTRL+C flag every second
  while (!shutdown_flag)
    std::this_thread::sleep_for(std::chrono::seconds(1));
  sys.println(caf::term::cyan, "received CTRL+C");
  // shutdown actors
  anon_send_exit(coordinator, caf::exit_reason::user_shutdown);
  // await actors
  act.sa_handler = [](int) { abort(); };
  set_sighandler();
  sys.println(caf::term::cyan, "await CURL; this may take a while "
                               "(press CTRL+C again to abort)");
  self->await_all_other_actors_done();
  // shutdown CURL
  curl_global_cleanup();
}

CAF_MAIN(caf::id_block::curl_fuse, caf::io::middleman)
