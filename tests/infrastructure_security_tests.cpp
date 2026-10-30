#include "simulator/infrastructure_security.hpp"
#include <iostream>
#include <set>
#include <string_view>
namespace {
int checks=0,failures=0;
void expect(bool ok,std::string_view label){++checks;if(!ok){++failures;std::cerr<<"FAILED "<<label<<'\n';}}
bool has(std::string_view text,std::string_view part){return text.find(part)!=std::string_view::npos;}
}
int main(){
  using namespace simulator::infrastructure_security;
  expect(scenarios().size()==28,"inventory size");
  std::set<std::string_view> ids;
  for(const auto& s:scenarios()){
    expect(ids.insert(s.id).second,"unique id");
    expect(!s.category.empty()&&!s.risk.empty()&&!s.control.empty()&&!s.failing_gate.empty(),"scenario complete");
    const auto blocked=simulate(s.id,"reviewer");
    expect(has(blocked,"\"decision\":\"no_go\""),"negative control blocks");
    expect(has(blocked,s.failing_gate),"failed gate matches scenario");
    expect(has(blocked,"\"network_access\":false")&&has(blocked,"\"side_effects\":false"),"offline only");
    expect(has(assess(s.id,"reviewer"),s.control),"reviewer receives safe control text");
    expect(assess(s.id,"viewer")==assess("missing","reviewer"),"uniform unauthorized disclosure");
    expect(simulate(s.id,"viewer")==simulate("missing","reviewer"),"uniform unknown disclosure");
  }
  expect(check_policy(PolicyEvidence{}).allowed_preview,"positive default fixture");
  PolicyEvidence e;
  e.role="operator";expect(check_policy(e).failed_gate=="role","role guard");e={};
  e.cloud_identity="admin";expect(check_policy(e).failed_gate=="cloud_identity","IAM guard");e={};
  e.namespace_scope="other_namespace";expect(check_policy(e).failed_gate=="namespace_scope","namespace guard");e={};
  e.service_account="default_auto_mount";expect(check_policy(e).failed_gate=="service_account","token guard");e={};
  e.rbac="wildcard_write";expect(check_policy(e).failed_gate=="rbac","rbac guard");e={};
  e.admission="disabled";expect(check_policy(e).failed_gate=="admission","admission guard");e={};
  e.pod_privilege="privileged";expect(check_policy(e).failed_gate=="pod_privilege","privileged guard");e={};
  e.gpu_isolation="unrestricted";expect(check_policy(e).failed_gate=="gpu_isolation","GPU guard");e={};
  e.resource_limits=false;expect(check_policy(e).failed_gate=="resource_limits","resources guard");e={};
  e.no_external_access=false;expect(check_policy(e).failed_gate=="external_access","offline boundary guard");e={};
  e.policy_source="model_output";expect(check_policy(e).failed_gate=="policy_source","untrusted model data guard");e={};
  expect(has(baseline("reviewer"),"\"decision\":\"allow_preview\""),"positive read-only preview");
  expect(baseline("viewer")==assess("missing","reviewer"),"baseline disclosure uniform");
  expect(has(evaluate("reviewer","approved_fixture","least_privilege","same_namespace","read_only_preview","approved"),"allow_preview"),"positive evaluation");
  expect(evaluate("reviewer","out_of_scope","least_privilege","same_namespace","read_only_preview","approved")==assess("missing","reviewer"),"scope denied");
  expect(evaluate("reviewer","approved_fixture","admin","same_namespace","read_only_preview","approved")==assess("missing","reviewer"),"admin denied");
  expect(evaluate("reviewer","approved_fixture","least_privilege","other_namespace","read_only_preview","approved")==assess("missing","reviewer"),"cross namespace denied");
  expect(has(list_scenarios(),"\"scenarios\":[")&&has(list_scenarios(),"\"network_access\":false"),"inventory format");
  expect(has(report(),"\"real_cloud_checks\":0")&&has(report(),"\"scenario_count\":28"),"honest report");
  if(failures){std::cerr<<failures<<"/"<<checks<<" failed\n";return 1;}
  std::cout<<"Infrastructure safety fixtures: "<<checks<<" checks passed\n";
}
