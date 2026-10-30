#include "simulator/a2a_security.hpp"
#include <iostream>
#include <set>
#include <string_view>
namespace {
int checked=0, failures=0;
void check(bool result, std::string_view label) { ++checked; if(!result) {++failures;std::cerr<<"FAIL: "<<label<<'\n';} }
bool has(std::string_view s,std::string_view needle){return s.find(needle)!=std::string_view::npos;}
}
int main() {
  using namespace simulator::a2a_security;
  check(scenarios().size()==24,"24 scenarios");
  std::set<std::string_view> ids;
  for(const auto& s:scenarios()) {
    check(ids.insert(s.id).second,"unique scenario");
    check(!s.category.empty()&&!s.threat.empty()&&!s.control.empty()&&!s.failed_gate.empty(),"full case");
    const auto denied=simulate(s.id,"reviewer");
    check(has(denied,"\"decision\":\"deny\""),"negative denied");
    check(has(denied,s.failed_gate),"correct gate denied");
    check(has(denied,"\"network_access\":false")&&has(denied,"\"side_effects\":false"),"no external effects");
    check(has(assess(s.id,"reviewer"),s.control),"reviewer control preview");
    check(assess(s.id,"viewer")==assess("missing","reviewer"),"uniform unknown assessment");
    check(simulate(s.id,"viewer")==simulate("missing","reviewer"),"uniform unknown simulation");
  }
  DelegationEvidence e;
  check(verify_fixture(e).accepted,"read-only baseline permits preview");
  e.tenant_scope="other_tenant";check(verify_fixture(e).failed_gate=="tenant_scope","cross tenant denied");e=DelegationEvidence{};
  e.session_scope="other_session";check(verify_fixture(e).failed_gate=="session_scope","cross session denied");e=DelegationEvidence{};
  e.role="viewer";check(verify_fixture(e).failed_gate=="role","unknown role denied");e=DelegationEvidence{};
  e.capability="write_database";check(verify_fixture(e).failed_gate=="capability","write capability denied");e=DelegationEvidence{};
  e.approval="none";check(verify_fixture(e).failed_gate=="approval","missing approval denied");e=DelegationEvidence{};
  e.handoff_depth=4;check(verify_fixture(e).failed_gate=="hop_limit","hop bound");e=DelegationEvidence{};
  e.message_bytes=9000;check(verify_fixture(e).failed_gate=="message_schema","message length bound");e=DelegationEvidence{};
  e.card_signature=false;check(verify_fixture(e).failed_gate=="card_signature","card signature policy");e=DelegationEvidence{};
  e.workflow_integrity=false;check(verify_fixture(e).failed_gate=="workflow_integrity","workflow step gate");e=DelegationEvidence{};
  e.credential_forwarding=false;check(verify_fixture(e).failed_gate=="credential_forwarding","credential isolation gate");e=DelegationEvidence{};
  e.peer_auth=false;check(verify_fixture(e).failed_gate=="peer_auth","peer authentication gate");e=DelegationEvidence{};
  e.artifact_integrity=false;check(verify_fixture(e).failed_gate=="artifact_integrity","artifact integrity gate");e=DelegationEvidence{};
  check(has(delegate("operator","same_tenant","same_session","read_only","approved"),"allow_preview"),"safe preview");
  check(has(delegate("operator","other_tenant","same_session","read_only","approved"),"\"decision\":\"deny\""),"remote tenant denied");
  check(has(delegate("operator","same_tenant","same_session","privileged","approved"),"\"decision\":\"deny\""),"privileged denied");
  check(has(delegate("operator","same_tenant","same_session","read_only","none"),"\"decision\":\"deny\""),"unapproved denied");
  check(baseline("viewer")==assess("missing","reviewer"),"unauthorized baseline uniform");
  check(has(baseline("reviewer"),"\"live_tasks_executed\":0"),"no tasks executed");
  check(has(list_scenarios(),"synthetic_multiagent_security"),"explicit fixture");
  check(has(report(),"\"measured_attack_success_rate\":null"),"no invented measurements");
  if(failures){std::cerr<<failures<<"/"<<checked<<" failed\n"; return 1;}
  std::cout<<"A2A control fixtures: "<<checked<<" assertions passed\n";
}
