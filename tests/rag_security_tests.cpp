#include "simulator/rag_security.hpp"
#include <set>
#include <iostream>
#include <cstdlib>
#include <string>
using namespace simulator::rag_security;
int main(){
 int checks=0;
 auto need=[&](bool value,const char* what){++checks;if(!value){std::cerr<<what<<" FAILED\n";std::exit(1);}};
 need(scenarios().size()==30,"scenario count");
 need(check(Evidence{}).allowed,"allow vetted fixture");
 std::set<std::string_view> ids;
 for(const auto& c:scenarios()){
   need(ids.insert(c.id).second,"unique ID");
   need(!c.control.empty()&&!c.risk.empty(),"controls described");
   const auto decision=check(failing_fixture(c.failing_gate));
   need(!decision.allowed,"fail closed scenario");
   need(decision.failed_gate==c.failing_gate,"matching gate");
   need(simulate(c.id,"reviewer").find("\"decision\":\"no_go\"")!=std::string::npos,"negative decision");
   need(simulate(c.id,"reviewer").find("\"network_access\":false")!=std::string::npos,"offline guarantee");
   need(assess(c.id,"viewer")==assess("not_real","viewer"),"opaque viewer response");
 }
 need(!check(failing_fixture("no_live_access")).allowed,"no live access");
 need(baseline("reviewer").find("allow_preview")!=std::string::npos,"approved fixture");
 need(baseline("viewer")==assess("not_real","viewer"),"hide baseline from viewer");
 need(evaluate("reviewer","same_tenant","verified_source","consistent_before_retrieval","delimited_untrusted","redacted_typed").find("allow_preview")!=std::string::npos,"approved metadata");
 need(evaluate("reviewer","other_tenant","verified_source","consistent_before_retrieval","delimited_untrusted","redacted_typed").find("\"decision\":\"deny\"")!=std::string::npos,"deny other tenant");
 need(evaluate("reviewer","same_tenant","anonymous_source","consistent_before_retrieval","delimited_untrusted","redacted_typed").find("deny")!=std::string::npos,"unverified provenance");
 need(evaluate("reviewer","same_tenant","verified_source","metadata_acl_mismatch","delimited_untrusted","redacted_typed").find("deny")!=std::string::npos,"ACL mismatch");
 need(evaluate("reviewer","same_tenant","verified_source","consistent_before_retrieval","elevated_document","redacted_typed").find("deny")!=std::string::npos,"untrusted document");
 need(evaluate("reviewer","same_tenant","verified_source","consistent_before_retrieval","delimited_untrusted","private_disclosure").find("deny")!=std::string::npos,"output blocked");
 const auto allowed=retrieve("reviewer","same_tenant","public","knowledge_preview");
 need(allowed.find("synthetic-guide-001")!=std::string::npos,"safe source attribution");
 need(allowed.find("UNREACHABLE")==std::string::npos,"not leaking protected rows");
 need(allowed.find("chunks_returned\":1")!=std::string::npos,"bounded response");
 need(retrieve("reviewer","other_tenant","public","knowledge_preview")==retrieve("viewer","same_tenant","public","knowledge_preview"),"uniform no access");
 need(retrieve("reviewer","same_tenant","confidential","knowledge_preview").find("deny")!=std::string::npos,"classification");
 need(retrieve("reviewer","same_tenant","public","export").find("deny")!=std::string::npos,"purpose limited");
 need(simulate("unknown","reviewer")==assess("unknown","reviewer"),"opaque unknown");
 need(list().find("gradual_document_poisoning")!=std::string::npos,"cases exposed");
 need(report().find("\"scenario_count\":30")!=std::string::npos,"report count");
 std::cout<<"RAG policy: "<<checks<<" independent checks passed\n";
}
