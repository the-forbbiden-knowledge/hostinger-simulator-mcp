#include "simulator/simulator.hpp"
#include "simulator/agent_security.hpp"
#include "simulator/recon_security.hpp"
#include "simulator/capstone_security.hpp"
#include "simulator/supplychain_security.hpp"
#include "simulator/vector_security.hpp"
#include "simulator/a2a_security.hpp"
#include "simulator/threat_model.hpp"
#include "simulator/infrastructure_security.hpp"
#include "simulator/mcp_tool_security.hpp"
#include "simulator/rag_security.hpp"
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
  if (argc == 3 && std::string_view(argv[1]) == "a2a" && std::string_view(argv[2]) == "list") {
    std::cout << simulator::a2a_security::list_scenarios() << '\n'; return 0;
  }
  if (argc == 3 && std::string_view(argv[1]) == "a2a" && std::string_view(argv[2]) == "report") {
    std::cout << simulator::a2a_security::report() << '\n'; return 0;
  }
  if (argc == 4 && std::string_view(argv[1]) == "a2a" && std::string_view(argv[2]) == "baseline") {
    std::cout << simulator::a2a_security::baseline(argv[3]) << '\n'; return 0;
  }
  if (argc == 5 && std::string_view(argv[1]) == "a2a" && std::string_view(argv[2]) == "assess") {
    std::cout << simulator::a2a_security::assess(argv[3], argv[4]) << '\n'; return 0;
  }
  if (argc == 5 && std::string_view(argv[1]) == "a2a" && std::string_view(argv[2]) == "simulate") {
    std::cout << simulator::a2a_security::simulate(argv[3], argv[4]) << '\n'; return 0;
  }
  if (argc == 8 && std::string_view(argv[1]) == "a2a" && std::string_view(argv[2]) == "delegate") {
    std::cout << simulator::a2a_security::delegate(argv[3], argv[4], argv[5], argv[6], argv[7]) << '\n'; return 0;
  }
  if(argc==3 && std::string_view(argv[1])=="threat") {
    if(std::string_view(argv[2])=="list") {std::cout<<simulator::threat_model::list_scenarios()<<'\n';return 0;}
    if(std::string_view(argv[2])=="report") {std::cout<<simulator::threat_model::report()<<'\n';return 0;}
    if(std::string_view(argv[2])=="assumptions") {std::cout<<simulator::threat_model::assumptions()<<'\n';return 0;}
    if(std::string_view(argv[2])=="zones") {std::cout<<simulator::threat_model::zones()<<'\n';return 0;}
  }
  if(argc==4 && std::string_view(argv[1])=="threat") {
    if(std::string_view(argv[2])=="baseline") {std::cout<<simulator::threat_model::baseline(argv[3])<<'\n';return 0;}
    if(std::string_view(argv[2])=="brief") {std::cout<<simulator::threat_model::brief(argv[3])<<'\n';return 0;}
  }
  if(argc==5 && std::string_view(argv[1])=="threat") {
    if(std::string_view(argv[2])=="assess") {std::cout<<simulator::threat_model::assess(argv[3],argv[4])<<'\n';return 0;}
    if(std::string_view(argv[2])=="simulate") {std::cout<<simulator::threat_model::simulate(argv[3],argv[4])<<'\n';return 0;}
  }
  if(argc==8 && std::string_view(argv[1])=="threat" && std::string_view(argv[2])=="plan") {
    std::cout<<simulator::threat_model::plan(argv[3],argv[4],argv[5],argv[6],argv[7])<<'\n';return 0;
  }
  if(argc==3 && std::string_view(argv[1])=="infra") {
    if(std::string_view(argv[2])=="list") {std::cout<<simulator::infrastructure_security::list_scenarios()<<'\n';return 0;}
    if(std::string_view(argv[2])=="report") {std::cout<<simulator::infrastructure_security::report()<<'\n';return 0;}
  }
  if(argc==4 && std::string_view(argv[1])=="infra" && std::string_view(argv[2])=="baseline") {
    std::cout<<simulator::infrastructure_security::baseline(argv[3])<<'\n';return 0;
  }
  if(argc==5 && std::string_view(argv[1])=="infra") {
    if(std::string_view(argv[2])=="assess") {std::cout<<simulator::infrastructure_security::assess(argv[3],argv[4])<<'\n';return 0;}
    if(std::string_view(argv[2])=="simulate") {std::cout<<simulator::infrastructure_security::simulate(argv[3],argv[4])<<'\n';return 0;}
  }
  if(argc==9 && std::string_view(argv[1])=="infra" && std::string_view(argv[2])=="evaluate") {
    std::cout<<simulator::infrastructure_security::evaluate(argv[3],argv[4],argv[5],argv[6],argv[7],argv[8])<<'\n';return 0;
  }
  if(argc==3 && std::string_view(argv[1])=="mcp") {
    if(std::string_view(argv[2])=="list") {std::cout<<simulator::mcp_tool_security::list()<<'\n';return 0;}
    if(std::string_view(argv[2])=="report") {std::cout<<simulator::mcp_tool_security::report()<<'\n';return 0;}
  }
  if(argc==4 && std::string_view(argv[1])=="mcp" && std::string_view(argv[2])=="baseline") {
    std::cout<<simulator::mcp_tool_security::baseline(argv[3])<<'\n';return 0;
  }
  if(argc==5 && std::string_view(argv[1])=="mcp") {
    if(std::string_view(argv[2])=="assess") {std::cout<<simulator::mcp_tool_security::assess(argv[3],argv[4])<<'\n';return 0;}
    if(std::string_view(argv[2])=="simulate") {std::cout<<simulator::mcp_tool_security::simulate(argv[3],argv[4])<<'\n';return 0;}
  }
  if(argc==9 && std::string_view(argv[1])=="mcp" && std::string_view(argv[2])=="evaluate") {
    std::cout<<simulator::mcp_tool_security::evaluate(argv[3],argv[4],argv[5],argv[6],argv[7],argv[8])<<'\n';return 0;
  }
  if(argc==3 && std::string_view(argv[1])=="rag") {
    if(std::string_view(argv[2])=="list") {std::cout<<simulator::rag_security::list()<<'\n';return 0;}
    if(std::string_view(argv[2])=="report") {std::cout<<simulator::rag_security::report()<<'\n';return 0;}
  }
  if(argc==4 && std::string_view(argv[1])=="rag" && std::string_view(argv[2])=="baseline") {
    std::cout<<simulator::rag_security::baseline(argv[3])<<'\n';return 0;
  }
  if(argc==5 && std::string_view(argv[1])=="rag") {
    if(std::string_view(argv[2])=="assess") {std::cout<<simulator::rag_security::assess(argv[3],argv[4])<<'\n';return 0;}
    if(std::string_view(argv[2])=="simulate") {std::cout<<simulator::rag_security::simulate(argv[3],argv[4])<<'\n';return 0;}
  }
  if(argc==7 && std::string_view(argv[1])=="rag" && std::string_view(argv[2])=="retrieve") {
    std::cout<<simulator::rag_security::retrieve(argv[3],argv[4],argv[5],argv[6])<<'\n';return 0;
  }
  if(argc==9 && std::string_view(argv[1])=="rag" && std::string_view(argv[2])=="evaluate") {
    std::cout<<simulator::rag_security::evaluate(argv[3],argv[4],argv[5],argv[6],argv[7],argv[8])<<'\n';return 0;
  }
  std::cerr << "Usage: simulator rag list | simulator rag report | simulator rag baseline <reviewer> | simulator rag assess <scenario_id> <reviewer> | simulator rag simulate <scenario_id> <reviewer> | simulator rag retrieve <reviewer> <same_tenant> <public> <knowledge_preview> | simulator rag evaluate <role> <tenant> <source> <acl> <context> <output> | simulator mcp list | simulator mcp report | simulator mcp baseline <reviewer> | simulator mcp assess <scenario_id> <reviewer> | simulator mcp simulate <scenario_id> <reviewer> | simulator mcp evaluate <role> <tenant> <source> <registry> <mutation> <approval> | simulator infra list | simulator infra report | simulator infra baseline <reviewer> | simulator infra assess <scenario_id> <reviewer> | simulator infra simulate <scenario_id> <reviewer> | simulator infra evaluate <role> <scope> <cloud_identity> <namespace> <activity> <approval> | simulator threat list | simulator threat assumptions | simulator threat zones | simulator threat report | simulator threat brief <reviewer> | simulator threat baseline <reviewer> | simulator threat assess <scenario_id> <reviewer> | simulator threat simulate <scenario_id> <reviewer> | simulator threat plan <role> <scope> <validated> <approved_window> <approved> | simulator a2a list | simulator a2a report | simulator a2a baseline <reviewer> | simulator a2a assess <scenario_id> <reviewer> | simulator a2a simulate <scenario_id> <reviewer> | simulator a2a delegate <role> <tenant_scope> <session_scope> <capability> <approval> | simulator vector list | simulator vector report | simulator vector baseline <reviewer> | simulator vector assess <scenario_id> <reviewer> | simulator vector simulate <scenario_id> <reviewer> | simulator vector query <role> <tenant_scope> <classification> <purpose> <result_kind> | simulator supplychain list | simulator supplychain report | simulator supplychain baseline <reviewer> | simulator supplychain assess <scenario_id> <reviewer> | simulator supplychain simulate <scenario_id> <reviewer> | simulator capstone list | simulator capstone report | simulator capstone assess <scenario_id> <reviewer> | simulator capstone simulate <scenario_id> <source> <role> <scope> <approval> | simulator recon list | simulator recon inspect <scenario_id> <viewer|operator|reviewer> | simulator list | simulator call <tool_name> <resource_id> | simulator security list | simulator security evaluate <source> <action> <role> <session_scope> [none|approved] | simulator security call <source> <role> <scope> <approval> <tool> <resource>\n";
  return 64;
}
