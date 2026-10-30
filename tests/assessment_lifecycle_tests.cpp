#include "simulator/assessment_lifecycle.hpp"
#include <cstdlib>
#include <iostream>
#include <set>
#include <string>
#include <string_view>
using namespace simulator::assessment_lifecycle;
int main() {
 int checks=0;
 auto require=[&](bool ok,const char* label){++checks;if(!ok){std::cerr<<"FAIL: "<<label<<'\n';std::exit(1);}};
 require(transitions().size()==6,"six ordered transitions");
 require(scenarios().size()==8,"eight negative scenarios");
 std::set<std::string_view> ids;
 for(const auto& s:scenarios()){
   require(ids.insert(s.id).second,"unique scenario");
   auto result=simulate(s.id,"reviewer");
   require(result.find("\"decision\":\"no_go\"")!=std::string::npos,"negative");
   require(result.find(std::string(s.expected_gate))!=std::string::npos,"gate");
   require(result.find("\"network_access\":false")!=std::string::npos,"offline");
   require(simulate(s.id,"viewer")==simulate("missing","viewer"),"opaque unauthorized");
 }
 for(const auto& step:transitions()){
   require(check("reviewer","same_tenant",step.current,step.next,step.required_evidence,"approved_read_only","redacted_trace").allowed,"allowed transition");
   const auto d=check("reviewer","same_tenant",step.current,step.next,"unverified","approved_read_only","redacted_trace");
   require(!d.allowed&&d.failed_gate=="evidence","evidence required");
   require(advance("reviewer","same_tenant",step.current,step.next,step.required_evidence,"approved_read_only","redacted_trace").find("\"state_persisted\":false")!=std::string::npos,"no state write");
 }
 require(!check("reviewer","same_tenant","closed","scoping","scope_verified","approved_read_only","redacted_trace").allowed,"no reentry");
 require(!check("reviewer","same_tenant","scoping","closed","scope_verified","approved_read_only","redacted_trace").allowed,"no skipped phase");
 require(!check("reviewer","other_tenant","scoping","threat_modeling","scope_verified","approved_read_only","redacted_trace").allowed,"tenant");
 require(!check("reviewer","same_tenant","independent_retest","closed","retest_missing","approved_read_only","redacted_trace").allowed,"retest");
 require(baseline("reviewer").find("allow_preview")!=std::string::npos,"baseline");
 require(baseline("viewer")==simulate("unknown","viewer"),"opaque");
 require(simulate("unknown","reviewer")==simulate("unknown","viewer"),"unknown");
 require(report().find("\"live_tests\":0")!=std::string::npos,"no real test");
 require(list().find("independent_retest")!=std::string::npos,"inventory");
 std::cout<<"Assessment lifecycle policy: "<<checks<<" assertions passed\n";
}
