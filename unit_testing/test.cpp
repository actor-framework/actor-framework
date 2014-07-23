#include <atomic>

#include "test.hpp"
#include "caf/all.hpp"

using namespace std;
using namespace caf;

namespace {
atomic<size_t> s_error_count{0};
}

size_t caf_error_count() { return s_error_count; }

void caf_inc_error_count() { ++s_error_count; }

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
