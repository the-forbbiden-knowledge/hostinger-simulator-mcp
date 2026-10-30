#include "simulator/threat_model.hpp"
#include <string>
#include <vector>

namespace simulator::threat_model {
namespace {
std::string deny() {
  // Unknown and unauthorized resources share one response to avoid existence leaks.
  return R"({"ok":true,"mode":"simulation_only","decision":"deny","reason":"not_authorized_or_unavailable","network_access":false,"side_effects":false})";
}
PlanEvidence violated(std::string_view gate) {
  PlanEvidence e;
  if(gate=="scope") e.scope="out_of_scope";
  else if(gate=="tenant_scope") e.tenant_scope="other_tenant";
  else if(gate=="evidence_state") e.evidence_state="hypothesis";
  else if(gate=="confidence") e.confidence="low";
  else if(gate=="engagement_window") e.engagement_window="outside_window";
  else if(gate=="approval") e.approval="none";
  else if(gate=="activity") e.activity="live_write";
  else if(gate=="provenance") e.provenance=false;
  else if(gate=="corroboration") e.independent_corroboration=false;
  else if(gate=="trust_boundaries") e.trust_boundaries_mapped=false;
  else if(gate=="asset_classification") e.crown_jewel_classified=false;
  else if(gate=="control_owner") e.control_owner_assigned=false;
  else if(gate=="risk_review") e.risk_review_complete=false;
  else if(gate=="data_minimization") e.data_minimized=false;
  else if(gate=="event_reporting") e.report_required_events=false;
  else if(gate=="audit") e.audit_log_enabled=false;
  else if(gate=="negative_findings") e.negative_findings_recorded=false;
  else if(gate=="stakeholder_signoff") e.owner_confirmed=false;
  else if(gate=="destructive_actions") e.no_destructive_actions=false;
  else if(gate=="external_access") e.no_external_access=false;
  else if(gate=="path_hops") e.path_hops=9;
  else if(gate=="time_budget") e.remaining_minutes=0;
  return e;
}
} // namespace

EvidenceState advance(EvidenceState current,EvidenceAction action,
                      bool attributable,bool corroborated) noexcept {
  if(current==EvidenceState::invalidated) return EvidenceState::invalidated;
  if(action==EvidenceAction::refute && attributable && corroborated) return EvidenceState::invalidated;
  if(action==EvidenceAction::hypothesize && current==EvidenceState::observed)
    return EvidenceState::hypothesis;
  if(action==EvidenceAction::corroborate && current==EvidenceState::hypothesis &&
     attributable && corroborated) return EvidenceState::validated;
  return current;
}
std::string_view state_name(EvidenceState state) noexcept {
  switch(state) {
    case EvidenceState::observed: return "observed";
    case EvidenceState::hypothesis: return "hypothesis";
    case EvidenceState::validated: return "validated";
    case EvidenceState::invalidated: return "invalidated";
  }
  return "unknown";
}
Decision check_plan(const PlanEvidence& e) noexcept {
  if(e.role!="reviewer" && e.role!="operator") return {false,"role"};
  if(e.scope!="approved_fixture") return {false,"scope"};
  if(e.tenant_scope!="same_tenant") return {false,"tenant_scope"};
  if(e.evidence_state!="validated") return {false,"evidence_state"};
  if(e.confidence!="high") return {false,"confidence"};
  if(e.engagement_window!="approved_window") return {false,"engagement_window"};
  if(e.approval!="approved") return {false,"approval"};
  if(e.activity!="read_only_preview") return {false,"activity"};
  if(e.path_hops<0 || e.path_hops>4) return {false,"path_hops"};
  if(e.remaining_minutes<1 || e.remaining_minutes>480) return {false,"time_budget"};
  if(!e.provenance) return {false,"provenance"};
  if(!e.independent_corroboration) return {false,"corroboration"};
  if(!e.trust_boundaries_mapped) return {false,"trust_boundaries"};
  if(!e.crown_jewel_classified) return {false,"asset_classification"};
  if(!e.control_owner_assigned) return {false,"control_owner"};
  if(!e.risk_review_complete) return {false,"risk_review"};
  if(!e.data_minimized) return {false,"data_minimization"};
  if(!e.report_required_events) return {false,"event_reporting"};
  if(!e.audit_log_enabled) return {false,"audit"};
  if(!e.negative_findings_recorded) return {false,"negative_findings"};
  if(!e.owner_confirmed) return {false,"stakeholder_signoff"};
  if(!e.no_destructive_actions) return {false,"destructive_actions"};
  if(!e.no_external_access) return {false,"external_access"};
  return {true,"none"};
}
const std::vector<Scenario>& scenarios() {
  static const std::vector<Scenario> cases{
    {"unverified_component_inventory","evidence","architecture_assumed_from_unverified_hints","require_correlated_evidence","evidence_state"},
    {"unattributed_osint","evidence","orphaned_observation_without_source","retain_source_provenance","provenance"},
    {"single_source_confirmation","evidence","one_unsupported_claim_promoted_to_fact","independent_corroboration","corroboration"},
    {"low_confidence_escalation","confidence","uncertainty_ignored_in_path_selection","confidence_gate","confidence"},
    {"out_of_scope_component","engagement","unapproved_asset_in_model","scope_is_hard_constraint","scope"},
    {"cross_tenant_asset_inference","isolation","foreign_tenant_crown_jewel_in_path","tenant_isolation","tenant_scope"},
    {"out_of_window_execution","engagement","simulation_outside_authorized_window","engagement_time_gate","engagement_window"},
    {"unapproved_path","engagement","path_not_authorized_by_owner","explicit_approval","approval"},
    {"unsafe_live_operation","activity","write_effects_requested_from_model","read_only_preview_only","activity"},
    {"unknown_boundary_transition","architecture","cross_zone_transition_without_mapping","map_trust_boundaries","trust_boundaries"},
    {"unclassified_critical_asset","asset","sensitive_target_without_classification","classify_critical_assets","asset_classification"},
    {"orphaned_mitigation","ownership","control_without_responsible_owner","assign_control_owner","control_owner"},
    {"unreviewed_detection_risk","risk","detection_and_impact_tradeoff_not_reviewed","risk_review_gate","risk_review"},
    {"real_personal_data_fixture","privacy","test_uses_unminimized_sensitive_data","synthetic_redacted_data","data_minimization"},
    {"unreported_tool_invocation","reporting","required_notification_missing","require_event_reporting","event_reporting"},
    {"unlogged_decision","audit","no_trust_boundary_or_policy_audit","auditable_decision_trace","audit"},
    {"missing_disconfirming_evidence","evidence","negative_finding_omitted_from_register","record_counterevidence","negative_findings"},
    {"stakeholder_unaware_of_scope","governance","risk_owner_did_not_confirm_roes","stakeholder_signoff","stakeholder_signoff"},
    {"destructive_activity_on_prod","safety","destructive_activity_proposed","non_destructive_test_only","destructive_actions"},
    {"unapproved_external_endpoint","network","planning_attempts_to_reach_external_target","no_external_calls","external_access"},
    {"unbounded_escalation_chain","planning","unbounded_trust_boundary_hops","bounded_path_planning","path_hops"},
    {"missing_time_budget","planning","path_requires_unavailable_time","finite_budget_and_go_no_go","time_budget"}
  };
  return cases;
}
std::string list_scenarios() {
  std::string out=R"({"ok":true,"mode":"simulation_only","scenario_kind":"synthetic_threat_model","network_access":false,"side_effects":false,"scenarios":[)";
  for(std::size_t i=0;i<scenarios().size();++i) {
    const auto& s=scenarios()[i];
    if(i) out+=',';
    out+="{\"id\":\""+std::string(s.id)+"\",\"category\":\""+std::string(s.category)+"\"}";
  }
  return out+"]}";
}
std::string assumptions() {
  return R"({"ok":true,"mode":"simulation_only","fixture":"synthetic_assumption_register","assumptions":[{"id":"H-01","claim":"local_simulator_exists","state":"validated","confidence":"high","basis":"compiled_test_fixture"},{"id":"H-02","claim":"future_MCP_gateway_is_available","state":"hypothesis","confidence":"low","basis":"architecture_plan_only"},{"id":"H-03","claim":"real_vector_database_is_connected","state":"invalidated","confidence":"high","basis":"explicit_no_network_boundary"}],"live_environment_observations":0,"network_access":false,"side_effects":false})";
}
std::string zones() {
  return R"({"ok":true,"mode":"simulation_only","fixture":"synthetic_trust_zones","zones":["untrusted_content","api_identity","agent_orchestration","mcp_tool_policy","native_fixture","private_data_and_keys"],"boundary_validation":"planned_for_network","live_network_scans":0,"network_access":false,"side_effects":false})";
}
std::string assess(std::string_view id,std::string_view role) {
  if(role!="reviewer") return deny();
  for(const auto& s:scenarios()) if(id==s.id) {
    return "{\"ok\":true,\"mode\":\"simulation_only\",\"decision\":\"allow_preview\",\"scenario\":\""+std::string(s.id)+"\",\"control\":\""+std::string(s.control)+"\",\"network_access\":false,\"side_effects\":false}";
  }
  return deny();
}
std::string simulate(std::string_view id,std::string_view role) {
  if(role!="reviewer") return deny();
  for(const auto& s:scenarios()) if(id==s.id) {
    const auto result=check_plan(violated(s.failing_gate));
    if(result.allowed_preview || result.failed_gate!=s.failing_gate) return deny();
    return "{\"ok\":true,\"mode\":\"simulation_only\",\"decision\":\""+std::string(s.failing_gate=="scope" || s.failing_gate=="tenant_scope" || s.failing_gate=="engagement_window" || s.failing_gate=="approval" || s.failing_gate=="activity" || s.failing_gate=="destructive_actions" || s.failing_gate=="external_access" ? "no_go" : "hold")+"\",\"scenario\":\""+std::string(s.id)+"\",\"blocked_gate\":\""+std::string(result.failed_gate)+"\",\"network_access\":false,\"side_effects\":false}";
  }
  return deny();
}
std::string baseline(std::string_view role) {
  if(role!="reviewer" || !check_plan(PlanEvidence{}).allowed_preview) return deny();
  return R"({"ok":true,"mode":"simulation_only","decision":"go_preview","scope":"approved_fixture","evidence_state":"validated","scenario":"read_only_local_review","real_attack_paths_executed":0,"network_access":false,"side_effects":false})";
}
std::string plan(std::string_view role,std::string_view scope,
                 std::string_view evidence_state,std::string_view window,
                 std::string_view approval) {
  PlanEvidence e;
  e.role=role;e.scope=scope;e.evidence_state=evidence_state;
  e.engagement_window=window;e.approval=approval;
  if(!check_plan(e).allowed_preview) return deny();
  return R"({"ok":true,"mode":"simulation_only","decision":"go_preview","scope":"approved_fixture","next_action":"read_only_local_review","network_access":false,"side_effects":false})";
}
std::string brief(std::string_view role) {
  if(role!="reviewer") return deny();
  return R"({"ok":true,"mode":"simulation_only","brief_version":"synthetic_1","verified_local_components":1,"assumptions_validated":1,"assumptions_unverified":1,"assumptions_invalidated":1,"priority_paths":[{"id":"TM-P01","state":"go_preview","scope":"local_fixture","next_action":"review_native_policy"},{"id":"TM-P02","state":"hold","blocker":"future_gateway_not_implemented"},{"id":"TM-P03","state":"no_go","blocker":"external_or_production_access_prohibited"}],"real_findings":0,"external_attack_path_testing":false,"network_access":false,"side_effects":false})";
}
std::string report() {
  return R"({"ok":true,"mode":"simulation_only","scenario_count":22,"evidence_register":"synthetic","risk_scoring":"not_measured","model_version":"synthetic_1","real_assets_tested":0,"real_attack_paths_executed":0,"network_access":false,"side_effects":false})";
}
} // namespace simulator::threat_model
