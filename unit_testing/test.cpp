#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>

#include "test.hpp"
#include "caf/all.hpp"
#include "caf/string_algorithms.hpp"

using namespace std;
using namespace caf;

namespace {

atomic<size_t> s_error_count{0};
std::mutex s_stdout_mtx;

class watchdog {
 public:
  watchdog() {
    m_thread = thread([&] {
      auto tp = chrono::high_resolution_clock::now() + chrono::seconds(10);
      std::unique_lock<mutex> guard{m_mtx};
      while (!m_canceled
             && m_cv.wait_until(guard, tp) != cv_status::timeout) {
        // spin
      }
      if (!m_canceled) {
        std::lock_guard<std::mutex> io_guard{s_stdout_mtx};
        cerr << "WATCHDOG: unit test did finish within 10s, abort" << endl;
        abort();
      }
    });
  }
  ~watchdog() {
    { // lifetime scope of guard
      std::lock_guard<std::mutex> guard{m_mtx};
      m_canceled = true;
      m_cv.notify_all();
    }
    m_thread.join();
  }

  volatile bool m_canceled = false;
  std::mutex m_mtx;
  std::condition_variable m_cv;
  std::thread m_thread;
};

watchdog* s_watchdog;

} // namespace <anonymous>

std::mutex& caf_stdout_mtx() {
  return s_stdout_mtx;
}

size_t caf_error_count() {
  return s_error_count;
}

void caf_inc_error_count() {
  ++s_error_count;
}

void caf_launch_watchdog() {
  s_watchdog = new watchdog;
}

void caf_cancel_watchdog() {
  delete s_watchdog;
}

string caf_fill4(size_t value) {
  string result = to_string(value);
  while (result.size() < 4) result.insert(result.begin(), '0');
  return result;
}

const char* caf_strip_path(const char* file) {
  auto res = file;
  auto i = file;
  for (char c = *i; c != '\0'; c = *++i) {
    if (c == '/') {
      res = i + 1;
    }
  }
  return res;
}

void caf_unexpected_message(const char* file, size_t line, message t) {
  CAF_PRINTERRC(file, line, "unexpected message: " << to_string(t));
}

void caf_unexpected_timeout(const char* file, size_t line) {
  CAF_PRINTERRC(file, line, "unexpected timeout");
}

vector<string> split(const string& str, char delim, bool keep_empties) {
  using namespace std;
  vector<string> result;
  stringstream strs{str};
  string tmp;
  while (getline(strs, tmp, delim)) {
    if (!tmp.empty() || keep_empties) result.push_back(move(tmp));
  }
  return result;
}

void verbose_terminate() {
  try {
    throw;
  }
  catch (std::exception& e) {
    CAF_PRINTERR("terminate called after throwing "
            << to_verbose_string(e));
  }
  catch (...) {
    CAF_PRINTERR("terminate called after throwing an unknown exception");
  }
  abort();
}

void set_default_test_settings() {
  set_terminate(verbose_terminate);
  cout.unsetf(ios_base::unitbuf);
}

std::thread run_program_impl(actor rc, const char* cpath, vector<string> args) {
  string path = cpath;
  replace_all(path, "'", "\\'");
  ostringstream oss;
  oss << "'" << path << "'";
  for (auto& arg : args) {
    oss << " " << arg;
  }
  oss << " 2>&1";
  string cmdstr = oss.str();
  return std::thread([cmdstr, rc] {
    string output;
    auto fp = popen(cmdstr.c_str(), "r");
    if (!fp) {
       CAF_PRINTERR("FATAL: command line failed: " << cmdstr);
       abort();
    }
    char buf[512];
    while (fgets(buf, sizeof(buf), fp)) {
      output += buf;
    }
    pclose(fp);
    anon_send(rc, output);
  });
}
