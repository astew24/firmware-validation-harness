#include "test_framework.hpp"

int main() {
  int failed = 0;
  for (const auto& c : tf::registry()) {
    tf::current_failed() = false;
    c.fn();
    std::printf("%s %s\n", tf::current_failed() ? "FAIL" : "ok  ", c.name);
    if (tf::current_failed()) ++failed;
  }
  std::printf("\n%zu tests, %d failed\n", tf::registry().size(), failed);
  return failed == 0 ? 0 : 1;
}
