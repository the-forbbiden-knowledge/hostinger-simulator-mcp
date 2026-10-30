#include "simulator/agent_security.hpp"
#include <iostream>
#include <string>
#include <string_view>

namespace {
int failures = 0;
void check(bool condition, std::string_view label) {
  if (!condition) { std::cerr << "FAIL: " << label << '\n'; ++failures; }
}
bool denied(std::string_view source, std::string_view action, std::string_view role="reviewer",
            std::string_view scope="same_session", std::string_view approval="approved") {
  return simulator::agent_security::evaluate(source,action,role,scope,approval).find("\"decision\":\"deny\"") != std::string::npos;
}
}
int main() {
  const auto& all=simulator::agent_security::scenarios();
  check(all.size()==14,"all 14 synthetic scenarios inventoried");
  check(simulator::agent_security::list_scenarios().find("shared_knowledge_poisoning")!=std::string::npos,"persistent poisoning scenario present");
  check(simulator::agent_security::list_scenarios().find("cross_session_memory_access")!=std::string::npos,"cross-session scenario present");
  check(simulator::agent_security::list_scenarios().find("hidden_web_content")!=std::string::npos,"hidden HTML scenario present");
  check(denied("user_input","execute_tool"),"direct injection cannot authorize tool execution");
  check(denied("document","execute_tool"),"document injection cannot authorize tool execution");
  check(denied("document_batch","execute_tool"),"multi-file injection cannot authorize tool execution");
  check(denied("web_page","execute_tool"),"hidden HTML cannot authorize tool execution");
  check(denied("source_code","execute_tool"),"source-code injection cannot authorize tool execution");
  check(denied("tool_output","execute_tool"),"tool output cannot authorize tool execution");
  check(denied("shared_memory","execute_tool"),"poisoned KB cannot authorize tool execution");
  check(denied("private_memory","execute_tool"),"prior session text cannot authorize tool execution");
  check(denied("trusted_application","execute_tool","operator","cross_session"),"cross session scope blocked");
  check(denied("trusted_application","reveal_secret"),"secret disclosure blocked regardless of role");
  check(denied("trusted_application","external_egress"),"external egress blocked regardless of role");
  check(denied("trusted_application","write_shared_memory","operator"),"operator cannot update shared KB");
  check(denied("trusted_application","write_shared_memory","reviewer","same_session","none"),"shared KB write requires approval");
  check(denied("trusted_application","read_private_memory","operator","same_session","none"),"private memory read requires approval");
  check(denied("trusted_application","execute_tool","viewer"),"viewer cannot execute tool");
  check(denied("trusted_application","execute_tool","operator","same_session","none"),"tool execution requires approval");
  check(denied("unknown","execute_tool"),"unknown context fail-closed");
  check(denied("trusted_application","mystery_action"),"unknown action fail-closed");
  check(!denied("document","read_fixture","viewer","same_session","none"),"synthetic read-only fixture can be read");
  check(!denied("trusted_application","execute_tool","operator","same_session","approved"),"authorized tool action returns mock preview");
  check(!denied("trusted_application","write_shared_memory","reviewer","same_session","approved"),"approved reviewer gets preview, not persistence");
  auto output=simulator::agent_security::evaluate("trusted_application","write_shared_memory","reviewer","same_session","approved");
  check(output.find("\"side_effects\":false")!=std::string::npos,"approved action has no side effects");
  check(simulator::agent_security::evaluate("tool_output","execute_tool","reviewer","same_session","approved")==
        simulator::agent_security::evaluate("tool_output","execute_tool","reviewer","same_session","approved"),
        "deterministic policy decision");
  check(simulator::agent_security::guarded_call("web_page","reviewer","same_session","approved","vps.list","demo").find("\"decision\":\"deny\"")!=std::string::npos,
        "guarded tool boundary denies web-origin instructions");
  check(simulator::agent_security::guarded_call("trusted_application","operator","same_session","approved","vps.list","demo").find("\"side_effects\":false")!=std::string::npos,
        "approved trusted worker may only preview a simulator tool");
  check(simulator::agent_security::guarded_call("trusted_application","operator","same_session","approved","missing.tool","demo").find("unknown_tool")!=std::string::npos,
        "guarded calls preserve tool allowlist");
  if(failures){std::cerr<<failures<<" agent security check(s) failed\n";return 1;}
  std::cout<<"Agent security policy tests passed (30 checks).\n";
  return 0;
}
