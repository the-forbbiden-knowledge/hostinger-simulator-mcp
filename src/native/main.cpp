#include "simulator/simulator.hpp"
#include "simulator/agent_security.hpp"
#include "simulator/recon_security.hpp"
#include "simulator/capstone_security.hpp"
#include "simulator/supplychain_security.hpp"
#include "simulator/vector_security.hpp"
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
  if (argc == 3 && std::string_view(argv[1]) == "recon" &&
      std::string_view(argv[2]) == "list") {
    std::cout << simulator::recon_security::list_scenarios() << '\n';
    return 0;
  }
  if (argc == 5 && std::string_view(argv[1]) == "recon" &&
      std::string_view(argv[2]) == "inspect") {
    std::cout << simulator::recon_security::inspect(argv[3], argv[4]) << '\n';
    return 0; // denial is an expected synthetic policy outcome
  }
  if (argc == 3 && std::string_view(argv[1]) == "capstone" &&
      std::string_view(argv[2]) == "list") {
    std::cout << simulator::capstone_security::list_scenarios() << '\n';
    return 0;
  }
  if (argc == 3 && std::string_view(argv[1]) == "capstone" &&
      std::string_view(argv[2]) == "report") {
    std::cout << simulator::capstone_security::report() << '\n';
    return 0;
  }
  if (argc == 5 && std::string_view(argv[1]) == "capstone" &&
      std::string_view(argv[2]) == "assess") {
    std::cout << simulator::capstone_security::assess(argv[3], argv[4]) << '\n';
    return 0;
  }
  if (argc == 8 && std::string_view(argv[1]) == "capstone" &&
      std::string_view(argv[2]) == "simulate") {
    std::cout << simulator::capstone_security::simulate(argv[3], argv[4], argv[5], argv[6], argv[7]) << '\n';
    return 0;
  }
  if (argc == 3 && std::string_view(argv[1]) == "supplychain" &&
      std::string_view(argv[2]) == "list") {
    std::cout << simulator::supplychain_security::list_scenarios() << '\n';
    return 0;
  }
  if (argc == 3 && std::string_view(argv[1]) == "supplychain" &&
      std::string_view(argv[2]) == "report") {
    std::cout << simulator::supplychain_security::report() << '\n';
    return 0;
  }
  if (argc == 4 && std::string_view(argv[1]) == "supplychain" &&
      std::string_view(argv[2]) == "baseline") {
    std::cout << simulator::supplychain_security::baseline(argv[3]) << '\n';
    return 0;
  }
  if (argc == 5 && std::string_view(argv[1]) == "supplychain" &&
      std::string_view(argv[2]) == "assess") {
    std::cout << simulator::supplychain_security::assess(argv[3], argv[4]) << '\n';
    return 0;
  }
  if (argc == 5 && std::string_view(argv[1]) == "supplychain" &&
      std::string_view(argv[2]) == "simulate") {
    std::cout << simulator::supplychain_security::simulate(argv[3], argv[4]) << '\n';
    return 0;
  }
  if (argc == 3 && std::string_view(argv[1]) == "vector" && std::string_view(argv[2]) == "list") {
    std::cout << simulator::vector_security::list_scenarios() << '\n'; return 0;
  }
  if (argc == 3 && std::string_view(argv[1]) == "vector" && std::string_view(argv[2]) == "report") {
    std::cout << simulator::vector_security::report() << '\n'; return 0;
  }
  if (argc == 4 && std::string_view(argv[1]) == "vector" && std::string_view(argv[2]) == "baseline") {
    std::cout << simulator::vector_security::baseline(argv[3]) << '\n'; return 0;
  }
  if (argc == 5 && std::string_view(argv[1]) == "vector" && std::string_view(argv[2]) == "assess") {
    std::cout << simulator::vector_security::assess(argv[3], argv[4]) << '\n'; return 0;
  }
  if (argc == 5 && std::string_view(argv[1]) == "vector" && std::string_view(argv[2]) == "simulate") {
    std::cout << simulator::vector_security::simulate(argv[3], argv[4]) << '\n'; return 0;
  }
  if (argc == 8 && std::string_view(argv[1]) == "vector" && std::string_view(argv[2]) == "query") {
    std::cout << simulator::vector_security::query(argv[3], argv[4], argv[5], argv[6], argv[7]) << '\n'; return 0;
  }
  std::cerr << "Usage: simulator vector list | simulator vector report | simulator vector baseline <reviewer> | simulator vector assess <scenario_id> <reviewer> | simulator vector simulate <scenario_id> <reviewer> | simulator vector query <role> <tenant_scope> <classification> <purpose> <result_kind> | simulator supplychain list | simulator supplychain report | simulator supplychain baseline <reviewer> | simulator supplychain assess <scenario_id> <reviewer> | simulator supplychain simulate <scenario_id> <reviewer> | simulator capstone list | simulator capstone report | simulator capstone assess <scenario_id> <reviewer> | simulator capstone simulate <scenario_id> <source> <role> <scope> <approval> | simulator recon list | simulator recon inspect <scenario_id> <viewer|operator|reviewer> | simulator list | simulator call <tool_name> <resource_id> | simulator security list | simulator security evaluate <source> <action> <role> <session_scope> [none|approved] | simulator security call <source> <role> <scope> <approval> <tool> <resource>\n";
  return 64;
}
