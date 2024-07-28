#include "vectorclassification.h"
#include <cstdlib>
#include <functional>
#include <iostream>
#include <string_view>
using namespace std::literals;
bool run_basic(std::function<void(const char *&start, const char *end)> f) {
  std::string_view view =
      "Hello, World! I think that you have to be fair: < baba > &lt; \r   \0 "
      "Hello, World! I think that you have to be fair:"sv;
  const char *start = view.data();
  const char *end = start + view.size();
  f(start, end);
  if (*start != '<') {
    printf("expected '<'\n");
    return false;
  }
  start++;
  f(start, end);
  if (*start != '&') {
    printf("expected '&'\n");
    return false;
  }
  start++;
  f(start, end);
  if (*start != '\r') {
    printf("expected '\\r'\n");
    return false;
  }
  start++;
  f(start, end);
  if (*start != '\0') {
    printf("expected '\\0'\n");

    return false;
  }
  start++;

  f(start, end);
  if (start != end) {
    printf("expected end of string\n");
    return false;
  }
  printf("test completed\n");

  return true;
}

int main() {
  if (!run_basic(NaiveAdvanceString)) {
    std::cerr << "NaiveAdvanceString failed" << std::endl;
    return EXIT_FAILURE;
  } else {
    std::cout << "NaiveAdvanceString passed" << std::endl;
  }
#if defined(__aarch64__)
  if (!run_basic(AdvanceStringWebKit)) {
    std::cerr << "AdvanceStringWebKit failed" << std::endl;
    return EXIT_FAILURE;
  } else {
    std::cout << "AdvanceStringWebKit passed" << std::endl;
  }
  if (!run_basic(AdvanceStringChromium)) {
    std::cerr << "AdvanceStringChromium failed" << std::endl;
    return EXIT_FAILURE;
  } else {
    std::cout << "AdvanceStringChromium passed" << std::endl;
  }
#endif
  return EXIT_SUCCESS;
}
