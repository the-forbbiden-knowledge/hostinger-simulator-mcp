#include "simulator/assessment_lifecycle.hpp"
#include <algorithm>
namespace simulator::assessment_lifecycle {
namespace {
std::string deny() { return R"({"ok":false,"decision":"deny","error":"not_available_or_authorized","mode":"simulation_only","network_access":false,"side_effects":false})"; }
std::string preview(std::string_view stage) {
  return std::string(R"({"ok":true,"decision":"allow_preview","next_stage":")")+
    std::string(stage)+R"(","evidence":"synthetic_only","state_persisted":false,"mode":"simulation_only","network_access":false,"side_effects":false})";
}
}
const std::vector<PhaseTransition>& transitions() {
  static const std::vector<PhaseTransition> steps{
    {"scoping","threat_modeling","scope_verified"},
    {"threat_modeling","adversarial_preview","threats_reviewed"},
    {"adversarial_preview","analysis_reporting","tests_reproduced"},
    {"analysis_reporting","remediation","findings_correlated"},
    {"remediation","independent_retest","fix_reviewed"},
    {"independent_retest","closed","retest_passed"}
  }; return steps;
}
const std::vector<Scenario>& scenarios() {
  static const std::vector<Scenario> cases{
    {"skipped_scope","transition"},{"unverified_test","evidence"},
    {"premature_closure","transition"},{"missing_approval","approval"},
    {"untrusted_reviewer","role"},{"cross_tenant_assessment","tenant"},
    {"unreviewed_report","evidence"},{"missing_telemetry","telemetry"}
  }; return cases;
}
Decision check(std::string_view role, std::string_view tenant, std::string_view current,
               std::string_view next, std::string_view evidence,
               std::string_view approval, std::string_view telemetry) noexcept {
  if(role!="reviewer")return {false,"role"};
  if(tenant!="same_tenant")return {false,"tenant"};
  if(approval!="approved_read_only")return {false,"approval"};
  if(telemetry!="redacted_trace")return {false,"telemetry"};
  const auto& steps=transitions();
  const auto it=std::find_if(steps.begin(),steps.end(),[current,next](const PhaseTransition& s){
    return s.current==current && s.next==next;
  });
  if(it==steps.end())return {false,"transition"};
  if(evidence!=it->required_evidence)return {false,"evidence"};
  return {true,"none"};
}
std::string list() {
  std::string o=R"({"ok":true,"mode":"simulation_only","network_access":false,"steps":[)";
  bool first=true;
  for(const auto& s:transitions()){
    if (!first) o += ',';
    first = false;
    o+=R"({"from":")"+std::string(s.current)+R"(","to":")"+
      std::string(s.next)+R"(","required_evidence":")"+std::string(s.required_evidence)+R"("})";
  }
  return o+"]}";
}
std::string report() {
  return R"({"ok":true,"mode":"simulation_only","phases":7,"allowed_transitions":6,"negative_scenarios":8,"real_engagements":0,"live_tests":0,"validated_findings":0,"state_persisted":false,"network_access":false,"side_effects":false})";
}
std::string advance(std::string_view role,std::string_view tenant,std::string_view current,
                    std::string_view next,std::string_view evidence,
                    std::string_view approval,std::string_view telemetry) {
  return check(role,tenant,current,next,evidence,approval,telemetry).allowed?preview(next):deny();
}
std::string baseline(std::string_view role) {
  return advance(role,"same_tenant","scoping","threat_modeling","scope_verified","approved_read_only","redacted_trace");
}
std::string simulate(std::string_view id,std::string_view role) {
  if(role!="reviewer")return deny();
  Decision d{false,"unknown"};
  if(id=="skipped_scope")
    d=check(role,"same_tenant","scoping","analysis_reporting","scope_verified","approved_read_only","redacted_trace");
  else if(id=="unverified_test")
    d=check(role,"same_tenant","adversarial_preview","analysis_reporting","unverified","approved_read_only","redacted_trace");
  else if(id=="premature_closure")
    d=check(role,"same_tenant","analysis_reporting","closed","findings_correlated","approved_read_only","redacted_trace");
  else if(id=="missing_approval")
    d=check(role,"same_tenant","scoping","threat_modeling","scope_verified","missing","redacted_trace");
  else if(id=="untrusted_reviewer")
    d=check("viewer","same_tenant","scoping","threat_modeling","scope_verified","approved_read_only","redacted_trace");
  else if(id=="cross_tenant_assessment")
    d=check(role,"other_tenant","scoping","threat_modeling","scope_verified","approved_read_only","redacted_trace");
  else if(id=="unreviewed_report")
    d=check(role,"same_tenant","analysis_reporting","remediation","unreviewed","approved_read_only","redacted_trace");
  else if(id=="missing_telemetry")
    d=check(role,"same_tenant","scoping","threat_modeling","scope_verified","approved_read_only","missing");
  else return deny();
  if(d.allowed)return deny();
  return std::string(R"({"ok":true,"decision":"no_go","failed_gate":")")+
    std::string(d.failed_gate)+
    R"(","evidence":"synthetic_only","mode":"simulation_only","network_access":false,"side_effects":false})";
}
}  // namespace simulator::assessment_lifecycle
