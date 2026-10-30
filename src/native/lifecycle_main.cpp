#include "simulator/assessment_lifecycle.hpp"
#include <iostream>
#include <string_view>
int main(int argc, char** argv) {
  using namespace simulator::assessment_lifecycle;
  if (argc == 2 && std::string_view(argv[1]) == "list") {
    std::cout << list() << '\n'; return 0;
  }
  if (argc == 2 && std::string_view(argv[1]) == "report") {
    std::cout << report() << '\n'; return 0;
  }
  if (argc == 3 && std::string_view(argv[1]) == "baseline") {
    std::cout << baseline(argv[2]) << '\n'; return 0;
  }
  if (argc == 4 && std::string_view(argv[1]) == "simulate") {
    std::cout << simulate(argv[2], argv[3]) << '\n'; return 0;
  }
  if (argc == 9 && std::string_view(argv[1]) == "advance") {
    std::cout << advance(argv[2], argv[3], argv[4], argv[5], argv[6], argv[7], argv[8]) << '\n';
    return 0;
  }
  std::cerr << "Usage: lifecycle_cli list | report | baseline <role> | simulate <id> <role> | advance <role> <tenant> <current> <next> <evidence> <approval> <telemetry>\n";
  return 64;
}
