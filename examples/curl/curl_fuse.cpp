/******************************************************************************
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
 ******************************************************************************/

// C includes
#include <csignal>
#include <cstdlib>
#include <ctime>

// C++ includes
#include <iostream>
#include <random>
#include <string>
#include <vector>

// CAF
#include "caf/all.hpp"
#include "caf/io/all.hpp"

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

  CAF_ADD_TYPE_ID(curl_fuse, (std::vector<char>))

  CAF_ADD_ATOM(curl_fuse, read_atom);
  CAF_ADD_ATOM(curl_fuse, fail_atom);
  CAF_ADD_ATOM(curl_fuse, next_atom);
  CAF_ADD_ATOM(curl_fuse, reply_atom);
  CAF_ADD_ATOM(curl_fuse, finished_atom);

CAF_END_TYPE_ID_BLOCK(curl_fuse)

using namespace caf;

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
constexpr size_t num_curl_workers = 10;

// minimum delay between HTTP requests
constexpr int min_req_interval = 10;

// maximum delay between HTTP requests
constexpr int max_req_interval = 300;

// put everything into anonymous namespace (except main)
namespace {

// provides print utility, a name, and a parent
struct base_state {
  base_state(local_actor* thisptr) : self(thisptr) {
    // nop
  }

  actor_ostream print() {
    return aout(self) << color << name << " (id = " << self->id() << "): ";
  }

  virtual bool init(std::string m_name, std::string m_color) {
    name = std::move(m_name);
    color = std::move(m_color);
    print() << "started" << color::reset_endl;
    return true;
  }

  virtual ~base_state() {
    print() << "done" << color::reset_endl;
  }

  local_actor* self;
  std::string name;
  std::string color;
};

// encapsulates an HTTP request
behavior client_job(stateful_actor<base_state>* self, const actor& parent) {
  if (!self->state.init("client-job", color::blue))
    return {}; // returning an empty behavior terminates the actor
  self->send(parent, read_atom_v, "http://www.example.com/index.html",
             uint64_t{0}, uint64_t{4095});
  return {
    [=](reply_atom, const buffer_type& buf) {
      self->state.print() << "successfully received " << buf.size() << " bytes"
                          << color::reset_endl;
      self->quit();
    },
    [=](fail_atom) {
      self->state.print() << "failure" << color::reset_endl;
      self->quit();
    },
  };
}

struct client_state : base_state {
  client_state(local_actor* selfptr)
    : base_state(selfptr),
      count(0),
      re(rd()),
      dist(min_req_interval, max_req_interval) {
    // nop
  }

  size_t count;
  std::random_device rd;
  std::default_random_engine re;
  std::uniform_int_distribution<int> dist;
};

// spawns HTTP requests
behavior client(stateful_actor<client_state>* self, const actor& parent) {
  using std::chrono::milliseconds;
  self->link_to(parent);
  if (!self->state.init("client", color::green))
    return {}; // returning an empty behavior terminates the actor
  self->send(self, next_atom_v);
  return {[=](next_atom) {
    auto& st = self->state;
    st.print() << "spawn new client_job (nr. " << ++st.count << ")"
               << color::reset_endl;
    // client_job will use IO
    // and should thus be spawned in a separate thread
    self->spawn<detached + linked>(client_job, parent);
    // compute random delay until next job is launched
    auto delay = st.dist(st.re);
    self->delayed_send(self, milliseconds(delay), next_atom_v);
  }};
}

struct curl_state : base_state {
  curl_state(local_actor* selfptr) : base_state(selfptr) {
    // nop
  }

  ~curl_state() override {
    if (curl != nullptr)
      curl_easy_cleanup(curl);
  }

  static size_t callback(void* data, size_t bsize, size_t nmemb, void* userp) {
    size_t size = bsize * nmemb;
    auto& buf = reinterpret_cast<curl_state*>(userp)->buf;
    auto first = reinterpret_cast<char*>(data);
    auto last = first + bsize;
    buf.insert(buf.end(), first, last);
    return size;
  }

  bool init(std::string m_name, std::string m_color) override {
    curl = curl_easy_init();
    if (curl == nullptr)
      return false;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &curl_state::callback);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
    return base_state::init(std::move(m_name), std::move(m_color));
  }

  CURL* curl = nullptr;
  buffer_type buf;
};

// manages a CURL session
behavior curl_worker(stateful_actor<curl_state>* self, const actor& parent) {
  if (!self->state.init("curl-worker", color::yellow))
    return {}; // returning an empty behavior terminates the actor
  return {[=](read_atom, const std::string& fname, uint64_t offset,
              uint64_t range) -> message {
    auto& st = self->state;
    st.print() << "read" << color::reset_endl;
    for (;;) {
      st.buf.clear();
      // set URL
      curl_easy_setopt(st.curl, CURLOPT_URL, fname.c_str());
      // set range
      std::ostringstream oss;
      oss << offset << "-" << range;
      curl_easy_setopt(st.curl, CURLOPT_RANGE, oss.str().c_str());
      // set curl callback
      curl_easy_setopt(st.curl, CURLOPT_WRITEDATA,
                       reinterpret_cast<void*>(&st));
      // launch file transfer
      auto res = curl_easy_perform(st.curl);
      if (res != CURLE_OK) {
        st.print() << "curl_easy_perform() failed: " << curl_easy_strerror(res)
                   << color::reset_endl;
      } else {
        long hc = 0; // http return code
        curl_easy_getinfo(st.curl, CURLINFO_RESPONSE_CODE, &hc);
        switch (hc) {
          default:
            st.print() << "http error: download failed with "
                       << "'HTTP RETURN CODE': " << hc << color::reset_endl;
            break;
          case 200: // ok
          case 206: // partial content
            st.print() << "received " << st.buf.size()
                       << " bytes with 'HTTP RETURN CODE': " << hc
                       << color::reset_endl;
            // tell parent that this worker is done
            self->send(parent, finished_atom_v);
            return make_message(reply_atom_v, std::move(st.buf));
          case 404: // file does not exist
            st.print() << "http error: download failed with "
                       << "'HTTP RETURN CODE': 404 (file does "
                       << "not exist!)" << color::reset_endl;
        }
      }
      // avoid 100% cpu utilization if remote side is not accessible
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }};
}

struct master_state : base_state {
  master_state(local_actor* selfptr) : base_state(selfptr) {
    // nop
  }
  std::vector<actor> idle;
  std::vector<actor> busy;
};

behavior curl_master(stateful_actor<master_state>* self) {
  if (!self->state.init("curl-master", color::magenta))
    return {}; // returning an empty behavior terminates the actor
  // spawn workers
  for (size_t i = 0; i < num_curl_workers; ++i)
    self->state.idle.push_back(
      self->spawn<detached + linked>(curl_worker, self));
  auto worker_finished = [=] {
    auto sender = self->current_sender();
    auto last = self->state.busy.end();
    auto i = std::find(self->state.busy.begin(), last, sender);
    if (i == last)
      return;
    self->state.idle.push_back(*i);
    self->state.busy.erase(i);
    self->state.print() << "worker is done" << color::reset_endl;
  };
  self->state.print() << "spawned " << self->state.idle.size() << " worker(s)"
                      << color::reset_endl;
  return {
    [=](read_atom rd, std::string& str, uint64_t x, uint64_t y) {
      auto& st = self->state;
      st.print() << "received {'read'}" << color::reset_endl;
      // forward job to an idle worker
      actor worker = st.idle.back();
      st.idle.pop_back();
      st.busy.push_back(worker);
      self->delegate(worker, rd, std::move(str), x, y);
      st.print() << st.busy.size() << " active jobs" << color::reset_endl;
      if (st.idle.empty()) {
        // wait until at least one worker finished its job
        self->become(keep_behavior, [=](finished_atom) {
          worker_finished();
          self->unbecome();
        });
      }
    },
    [=](finished_atom) { worker_finished(); },
  };
}

// signal handling for ctrl+c
std::atomic<bool> shutdown_flag{false};

struct config : actor_system_config {
  config() {
    init_global_meta_objects<curl_fuse_type_ids>();
  }
};

} // namespace

void caf_main(actor_system& system, const config&) {
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
  scoped_actor self{system};
  // spawn client and curl_master
  auto master = self->spawn<detached>(curl_master);
  self->spawn<detached>(client, master);
  // poll CTRL+C flag every second
  while (!shutdown_flag)
    std::this_thread::sleep_for(std::chrono::seconds(1));
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
  // shutdown CURL
  curl_global_cleanup();
}

CAF_MAIN(io::middleman)
