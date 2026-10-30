#include "simulator/threat_model.hpp"
#include <iostream>
#include <set>
#include <string_view>
namespace {
int checked=0,failed=0;
void check(bool ok,std::string_view label){++checked;if(!ok){++failed;std::cerr<<"FAIL: "<<label<<'\n';}}
bool contains(std::string_view v,std::string_view needle){return v.find(needle)!=std::string_view::npos;}
}
int main(){
 using namespace simulator::threat_model;
 check(scenarios().size()==22,"22 distinct cases");
 std::set<std::string_view> ids;
 for(const auto& s:scenarios()){
  check(ids.insert(s.id).second,"unique id");
  check(!s.category.empty()&&!s.risk.empty()&&!s.control.empty()&&!s.failing_gate.empty(),"complete case");
  auto denied=simulate(s.id,"reviewer");
  check(contains(denied,"\"decision\":\"hold\"") || contains(denied,"\"decision\":\"no_go\""),"negative blocked");
  check(contains(denied,s.failing_gate),"specific gate blocked");
  check(contains(denied,"\"network_access\":false")&&contains(denied,"\"side_effects\":false"),"offline");
  check(contains(assess(s.id,"reviewer"),s.control),"control offered");
  check(assess(s.id,"viewer")==assess("missing","reviewer"),"uniform denial");
  check(simulate(s.id,"viewer")==simulate("missing","reviewer"),"unknown simulation uniform");
 }
 check(advance(EvidenceState::observed,EvidenceAction::hypothesize,false,false)==EvidenceState::hypothesis,"observation may hypothesize");
 check(advance(EvidenceState::hypothesis,EvidenceAction::corroborate,false,true)==EvidenceState::hypothesis,"no unattributed validation");
 check(advance(EvidenceState::hypothesis,EvidenceAction::corroborate,true,false)==EvidenceState::hypothesis,"no uncorroborated validation");
 check(advance(EvidenceState::hypothesis,EvidenceAction::corroborate,true,true)==EvidenceState::validated,"corroborated validation");
 check(advance(EvidenceState::hypothesis,EvidenceAction::refute,true,true)==EvidenceState::invalidated,"refutation");
 check(advance(EvidenceState::invalidated,EvidenceAction::corroborate,true,true)==EvidenceState::invalidated,"cannot revive refuted hypothesis");
 check(state_name(EvidenceState::validated)=="validated","state label");
 check(check_plan(PlanEvidence{}).allowed_preview,"local safe positive fixture");
 PlanEvidence e;
 e.scope="out_of_scope";check(check_plan(e).failed_gate=="scope","out of scope rejected");e=PlanEvidence{};
 e.evidence_state="hypothesis";check(check_plan(e).failed_gate=="evidence_state","hypothesis held");e=PlanEvidence{};
 e.engagement_window="outside_window";check(check_plan(e).failed_gate=="engagement_window","window gated");e=PlanEvidence{};
 e.role="viewer";check(check_plan(e).failed_gate=="role","untrusted role denied");e=PlanEvidence{};
 e.tenant_scope="other_tenant";check(check_plan(e).failed_gate=="tenant_scope","cross tenant denied");e=PlanEvidence{};
 e.path_hops=5;check(check_plan(e).failed_gate=="path_hops","bounded chain");e=PlanEvidence{};
 e.remaining_minutes=0;check(check_plan(e).failed_gate=="time_budget","finite budget");e=PlanEvidence{};
 e.report_required_events=false;check(check_plan(e).failed_gate=="event_reporting","mandatory reporting");e=PlanEvidence{};
 e.no_external_access=false;check(check_plan(e).failed_gate=="external_access","external access blocked");e=PlanEvidence{};
 check(contains(plan("reviewer","approved_fixture","validated","approved_window","approved"),"go_preview"),"safe plan");
 check(plan("reviewer","out_of_scope","validated","approved_window","approved")==assess("missing","reviewer"),"scope denial uniform");
 check(plan("reviewer","approved_fixture","hypothesis","approved_window","approved")==assess("missing","reviewer"),"hypothesis denial uniform");
 check(brief("viewer")==assess("missing","reviewer"),"brief requires reviewer");
 check(contains(brief("reviewer"),"\"state\":\"hold\"")&&contains(brief("reviewer"),"\"state\":\"no_go\""),"honest brief blockers");
 check(contains(assumptions(),"\"state\":\"invalidated\""),"contrary evidence retained");
 check(contains(zones(),"trust_zones"),"zones model");
 check(contains(report(),"\"real_assets_tested\":0"),"no real tests claimed");
 if(failed){std::cerr<<failed<<"/"<<checked<<" assertions failed\n";return 1;}
 std::cout<<"Threat planning fixtures: "<<checked<<" assertions passed\n";
}
