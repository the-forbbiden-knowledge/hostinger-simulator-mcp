#include "simulator/simulator.hpp"
#include "simulator/agent_security.hpp"
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
  if (argc == 3 && std::string_view(argv[1]) == "security" &&
      std::string_view(argv[2]) == "list") {
    std::cout << simulator::agent_security::list_scenarios() << '\n';
    return 0;
  }
  if (argc == 7 && std::string_view(argv[1]) == "security" &&
      std::string_view(argv[2]) == "evaluate") {
    const auto result = simulator::agent_security::evaluate(argv[3], argv[4], argv[5], argv[6], "none");
    std::cout << result << '\n';
    return result.find("invalid_policy_input") == std::string::npos ? 0 : 2;
  }
  if (argc == 8 && std::string_view(argv[1]) == "security" &&
      std::string_view(argv[2]) == "evaluate") {
    const auto result = simulator::agent_security::evaluate(argv[3], argv[4], argv[5], argv[6], argv[7]);
    std::cout << result << '\n';
    return result.find("invalid_policy_input") == std::string::npos ? 0 : 2;
  }
  if (argc == 9 && std::string_view(argv[1]) == "security" &&
      std::string_view(argv[2]) == "call") {
    const auto result = simulator::agent_security::guarded_call(argv[3], argv[4], argv[5], argv[6], argv[7], argv[8]);
    std::cout << result << '\n';
    return result.find("invalid_policy_input") == std::string::npos ? 0 : 2;
  }
  std::cerr << "Usage: simulator list | simulator call <tool_name> <resource_id> | simulator security list | simulator security evaluate <source> <action> <role> <session_scope> [none|approved] | simulator security call <source> <role> <scope> <approval> <tool> <resource>\n";
  return 64;
}
