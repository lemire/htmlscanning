
#include "performancecounters/benchmarker.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <string_view>

#include "vectorclassification.h"
std::string load_file_content(std::string filename) {
  std::ifstream file(filename);
  if (!file.is_open()) {
    std::cerr << "Error opening file: " << filename << std::endl;
    return "";
  }

  std::string content;
  std::string line;
  while (std::getline(file, line)) {
    content += line + "\n";
  }
  file.close();
  return content;
}

void pretty_print(size_t volume, size_t bytes, std::string name,
                  event_aggregate agg) {
  printf("%-40s : ", name.c_str());
  printf(" %5.2f GB/s ", bytes / agg.fastest_elapsed_ns());
  printf(" %5.1f Ma/s ", volume * 1000.0 / agg.fastest_elapsed_ns());
  printf(" %5.2f ns/d ", agg.fastest_elapsed_ns() / volume);
  if (collector.has_events()) {
    printf(" %5.2f GHz ", agg.fastest_cycles() / agg.fastest_elapsed_ns());
    printf(" %5.2f c/d ", agg.fastest_cycles() / volume);
    printf(" %5.2f i/d ", agg.fastest_instructions() / volume);
    printf(" %5.2f c/b ", agg.fastest_cycles() / bytes);
    printf(" %5.2f i/b ", agg.fastest_instructions() / bytes);
    printf(" %5.2f i/c ", agg.fastest_instructions() / agg.fastest_cycles());
  }
  printf("\n");
}

template <typename T> bool check(const char *start, const char *end) {
  const char *targets = "<&\r\0";
  const char *expected = start;
  expected =
      std::find_first_of(expected, end, targets, targets + strlen(targets));

  T m(start, end);
  while (m.advance()) {
    if (m.get() != expected) {
      return false;
    }
    m.consume();
    if (expected < end)
      expected++;
    expected =
        std::find_first_of(expected, end, targets, targets + strlen(targets));
  }
  if (expected != end) {
    return false;
  }
  return true;
}

void print_stats(const std::string& data, const char *targets = "<&\r\0") {
  size_t volume = data.size();
   const char *start = data.data();
   size_t matches = 0;
                   const char *end = start + data.size();
                   while (start < end) {
                     start = std::find_first_of(start, end, targets,
                                                targets + strlen(targets));
                     if (start < end) {
                       start++;
                       matches++;
                     }
                   }
  printf("# Volume: %zu B, matches = %zu, fraction %.3f %% \n", volume, matches, matches * 100.0 / volume);
}

void process(std::string filename) {
  std::string data = load_file_content(filename);
  print_stats(data);
  size_t volume = data.size();
  size_t size = 1;
  volatile uint64_t count = 0;
  const char *targets = "<&\r\0";
  size_t repeat = 1000;
  pretty_print(size * repeat, volume * repeat, "std",
               bench([&data, &count, targets, repeat]() {
                 for (size_t r = 0; r < repeat; r++) {

                   const char *start = data.data();
                   const char *end = start + data.size();
                   while (start < end) {
                     count = *start;
                     start = std::find_first_of(start, end, targets,
                                                targets + strlen(targets));
                     if (start < end)
                       start++;
                   }
                 }
               }));

  pretty_print(
      size * repeat, volume * repeat, "NaiveAdvanceString", bench([&data, &count, repeat]() {
        for (size_t r = 0; r < repeat; r++) {

          const char *start = data.data();
          const char *end = start + data.size();
          while (start < end) {
            count = *start; // volatile assignment (compiler cannot cheat)
            NaiveAdvanceString(start, end);
            if (start < end)
              start++;
          }
        }
      }));
#if defined(__aarch64__)
  {
    const char *start = data.data();
    const char *end = start + data.size();
    check<neon_match64>(start, end);
  }

  pretty_print(
      size * repeat, volume * repeat, "neon64",
      bench([&data, &count, repeat]() {
        for (size_t r = 0; r < repeat; r++) {

          const char *start = data.data();
          const char *end = start + data.size();
          neon_match64 m(start, end);
          while (m.advance()) {
            count = *m.get(); // volatile assignment (compiler cannot cheat)
            m.consume();
          }
        }
      }));
  pretty_print(
      size * repeat, volume * repeat,  "AdvanceStringWebKit", bench([&data, &count, repeat]() {
        for (size_t r = 0; r < repeat; r++) {
          const char *start = data.data();
          const char *end = start + data.size();
          while (start < end) {
            count = *start; // volatile assignment (compiler cannot cheat)
            AdvanceStringWebKit(start, end);
            if (start < end)
              start++;
          }
        }
      }));
  pretty_print(
      size * repeat, volume * repeat,  "AdvanceStringChromium", bench([&data, &count, repeat]() {
        for (size_t r = 0; r < repeat; r++) {
          const char *start = data.data();
          const char *end = start + data.size();
          while (start < end) {
            count = *start; // volatile assignment (compiler cannot cheat)
            AdvanceStringChromium(start, end);
            if (start < end)
              start++;
          }
        }
      }));
#endif
}

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <filename>" << std::endl;
    return EXIT_FAILURE;
  }
  process(argv[1]);
  return EXIT_SUCCESS;
}
