#include "simulator/simulator.hpp"
#include <iostream>
#include <string_view>

int main(int argc, char** argv) {
  if (argc == 2 && std::string_view(argv[1]) == "list") {
    std::cout << simulator::list_tools() << '\n';
    return 0;
  }
  if (argc == 4 && std::string_view(argv[1]) == "call") {
    const auto result = simulator::call_tool(argv[2], argv[3]);
    std::cout << result << '\n';
    return result.rfind("{\"ok\":true", 0) == 0 ? 0 : 2;
  }
  std::cerr << "Usage: simulator list | simulator call <tool_name> <resource_id>\n";
  return 64;
}
