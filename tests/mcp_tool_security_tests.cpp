#include "simulator/mcp_tool_security.hpp"
#include <iostream>
#include <cstdlib>
#include <set>
#include <string>
using namespace simulator::mcp_tool_security;
int main() {
  int checks=0;
  auto require=[&](bool ok,const char* message) {++checks;if(!ok){std::cerr<<message<<" failed\n"; std::exit(1);}};
  require(scenarios().size()==32,"scenario cardinality");
  require(check(Evidence{}).allowed,"trusted synthetic baseline");
  require(check(Evidence{}).failed_gate=="none","trusted gate result");
  std::set<std::string_view> names;
  for(const auto& c:scenarios()) {
    require(names.insert(c.id).second,"unique case identifier");
    require(!c.risk.empty() && !c.control.empty(),"documented risk and control");
    const auto r=check(failing_fixture(c.failing_gate));
    require(!r.allowed,"scenario denied");
    require(r.failed_gate==c.failing_gate,"correct failing gate");
    const auto response=simulate(c.id,"reviewer");
    require(response.find("\"decision\":\"no_go\"")!=std::string::npos,"simulation denied");
    require(response.find("\"network_access\":false")!=std::string::npos,"no network access");
    require(response.find("\"side_effects\":false")!=std::string::npos,"no side effects");
    require(assess(c.id,"reviewer").find(std::string(c.id))!=std::string::npos,"known reviewed scenario");
    require(assess(c.id,"viewer")==assess("unknown","viewer"),"uniform nonreviewer denial");
  }
  require(!check(failing_fixture("no_live_access")).allowed,"live effects forbidden");
  require(evaluate("reviewer","same_tenant","trusted_application","pinned_registry","none","approved").find("allow_preview")!=std::string::npos,"safe eval");
  require(evaluate("reviewer","other_tenant","trusted_application","pinned_registry","none","approved").find("\"decision\":\"deny\"")!=std::string::npos,"tenant denied");
  require(evaluate("reviewer","same_tenant","tool_result","pinned_registry","none","approved").find("\"decision\":\"deny\"")!=std::string::npos,"tool output not authority");
  require(evaluate("reviewer","same_tenant","trusted_application","unverified_registry","none","approved").find("\"decision\":\"deny\"")!=std::string::npos,"registry pinned");
  require(evaluate("reviewer","same_tenant","trusted_application","pinned_registry","production_write","approved").find("\"decision\":\"deny\"")!=std::string::npos,"no production mutation");
  require(evaluate("reviewer","same_tenant","trusted_application","pinned_registry","none","missing").find("\"decision\":\"deny\"")!=std::string::npos,"approval missing");
  require(simulate("unknown","reviewer")==assess("unknown","reviewer"),"unknown denied uniformly");
  require(list().find("description_instruction_poisoning")!=std::string::npos,"inventory");
  require(report().find("\"scenario_count\":32")!=std::string::npos,"report count");
  require(baseline("viewer")==assess("unknown","viewer"),"baseline restricted");
  std::cout<<"MCP tool policy: "<<checks<<" internal checks passed\n";
  return 0;
}
