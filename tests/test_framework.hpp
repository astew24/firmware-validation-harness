#pragma once

// Tiny test framework: no outside dependencies, so the suite builds anywhere
// a C++17 compiler does.

#include <cstdio>
#include <vector>

namespace tf {

struct Case {
  const char* name;
  void (*fn)();
};

inline std::vector<Case>& registry() {
  static std::vector<Case> r;
  return r;
}

inline bool& current_failed() {
  static bool failed = false;
  return failed;
}

struct Registrar {
  Registrar(const char* name, void (*fn)()) { registry().push_back({name, fn}); }
};

inline void fail(const char* file, int line, const char* expr) {
  std::printf("    FAILED %s:%d: %s\n", file, line, expr);
  current_failed() = true;
}

}  // namespace tf

#define TF_CAT2(a, b) a##b
#define TF_CAT(a, b) TF_CAT2(a, b)

#define TEST(name)                                                 \
  static void name();                                              \
  static const tf::Registrar TF_CAT(reg_, name)(#name, &name);     \
  static void name()

#define CHECK(cond)                                \
  do {                                             \
    if (!(cond)) {                                 \
      tf::fail(__FILE__, __LINE__, #cond);         \
      return;                                      \
    }                                              \
  } while (0)

#define CHECK_EQ(a, b)                             \
  do {                                             \
    if (!((a) == (b))) {                           \
      tf::fail(__FILE__, __LINE__, #a " == " #b);  \
      return;                                      \
    }                                              \
  } while (0)
