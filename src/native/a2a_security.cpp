#include "simulator/a2a_security.hpp"
#include <string>
#include <vector>
namespace simulator::a2a_security {
namespace {
std::string deny() {
  // Uniform response prevents existence-oracle leaks for unknown and unauthorized IDs.
  return R"({"ok":true,"mode":"simulation_only","decision":"deny","reason":"not_authorized_or_unavailable","network_access":false,"side_effects":false})";
}
DelegationEvidence negative_fixture(std::string_view gate) {
  DelegationEvidence e;
  if (gate=="tenant_scope") e.tenant_scope="other_tenant";
  else if (gate=="session_scope") e.session_scope="other_session";
  else if (gate=="registry_auth") e.registry_auth=false;
  else if (gate=="card_signature") e.card_signature=false;
  else if (gate=="card_identity") e.card_identity=false;
  else if (gate=="endpoint_origin") e.endpoint_origin=false;
  else if (gate=="transport_tls") e.transport_tls=false;
  else if (gate=="capability_allowlist") e.capability_allowlist=false;
  else if (gate=="least_privilege") e.least_privilege=false;
  else if (gate=="peer_auth") e.peer_auth=false;
  else if (gate=="hop_limit") e.hop_limit=false;
  else if (gate=="quorum") e.quorum=false;
  else if (gate=="task_owner") e.task_owner=false;
  else if (gate=="replay") e.replay=false;
  else if (gate=="workflow_integrity") e.workflow_integrity=false;
  else if (gate=="artifact_integrity") e.artifact_integrity=false;
  else if (gate=="safe_link") e.safe_link=false;
  else if (gate=="db_query") e.db_query=false;
  else if (gate=="untrusted_content") e.untrusted_content=false;
  else if (gate=="message_schema") e.message_schema=false;
  else if (gate=="callback_destination") e.callback_destination=false;
  else if (gate=="credential_forwarding") e.credential_forwarding=false;
  else if (gate=="tool_scope") e.tool_scope=false;
  else if (gate=="audit_trace") e.audit_trace=false;
  return e;
}
} // namespace
Decision verify_fixture(const DelegationEvidence& e) noexcept {
  // The order is intentional, stable and verified by negative tests.
  if (e.role!="reviewer" && e.role!="operator") return {false,"role"};
  if (e.tenant_scope!="same_tenant") return {false,"tenant_scope"};
  if (e.session_scope!="same_session") return {false,"session_scope"};
  if (e.capability!="read_only") return {false,"capability"};
  if (e.approval!="approved") return {false,"approval"};
  if (e.handoff_depth<0 || e.handoff_depth>3) return {false,"hop_limit"};
  if (e.message_bytes<1 || e.message_bytes>8192) return {false,"message_schema"};
  if (!e.registry_auth) return {false,"registry_auth"};
  if (!e.card_signature) return {false,"card_signature"};
  if (!e.card_identity) return {false,"card_identity"};
  if (!e.endpoint_origin) return {false,"endpoint_origin"};
  if (!e.transport_tls) return {false,"transport_tls"};
  if (!e.capability_allowlist) return {false,"capability_allowlist"};
  if (!e.least_privilege) return {false,"least_privilege"};
  if (!e.peer_auth) return {false,"peer_auth"};
  if (!e.hop_limit) return {false,"hop_limit"};
  if (!e.quorum) return {false,"quorum"};
  if (!e.task_owner) return {false,"task_owner"};
  if (!e.replay) return {false,"replay"};
  if (!e.workflow_integrity) return {false,"workflow_integrity"};
  if (!e.artifact_integrity) return {false,"artifact_integrity"};
  if (!e.safe_link) return {false,"safe_link"};
  if (!e.db_query) return {false,"db_query"};
  if (!e.untrusted_content) return {false,"untrusted_content"};
  if (!e.message_schema) return {false,"message_schema"};
  if (!e.callback_destination) return {false,"callback_destination"};
  if (!e.credential_forwarding) return {false,"credential_forwarding"};
  if (!e.tool_scope) return {false,"tool_scope"};
  if (!e.audit_trace) return {false,"audit_trace"};
  return {true,"none"};
}
const std::vector<Scenario>& scenarios() {
  static const std::vector<Scenario> items{
    {"unauthenticated_agent_registration","discovery","untrusted_agent_added_to_registry","registry_authentication","registry_auth"},
    {"untrusted_agent_card","discovery","agent_card_unverified_origin","verify_card_signature_policy","card_signature"},
    {"agent_card_identity_mismatch","identity","agent_card_claimed_identity_differs","bind_identity_to_trusted_registry","card_identity"},
    {"agent_card_host_redirection","discovery","agent_endpoint_dns_or_host_redirect","pin_approved_endpoint_and_tls_identity","endpoint_origin"},
    {"agent_card_endpoint_downgrade","transport","agent_endpoint_not_secure_transport","require_tls_transport","transport_tls"},
    {"capability_overclaim","capabilities","agent_declares_unauthorized_skills","server_owned_capability_allowlist","capability_allowlist"},
    {"orchestrator_privilege_inheritance","delegation","delegated_agent_inherits_excess_authority","downscope_per_delegated_task","least_privilege"},
    {"untrusted_peer_handoff","delegation","peer_handoff_without_validated_actor","authenticate_each_peer","peer_auth"},
    {"unbounded_agent_hops","delegation","recursive_agent_handoff","limit_handoff_depth","hop_limit"},
    {"swarm_quorum_bypass","coordination","decision_without_required_authorization","independent_quorum_policy","quorum"},
    {"cross_tenant_agent_task","isolation","foreign_tenant_task_context","server_derived_tenant_binding","tenant_scope"},
    {"cross_session_agent_task","isolation","foreign_session_task_context","session_owned_task_binding","session_scope"},
    {"task_owner_confusion","tasks","task_result_visible_to_unrelated_principal","verify_task_owner","task_owner"},
    {"task_replay","tasks","repeated_task_execution_from_stale_request","prevent_duplicate_task_nonce","replay"},
    {"workflow_integrity_bypass","workflow","mandatory_security_step_skipped","validate_workflow_state_machine","workflow_integrity"},
    {"output_artifact_tampering","artifacts","generated_file_modified_after_review","verify_output_artifact_digest","artifact_integrity"},
    {"display_target_mismatch","artifacts","visible_link_differs_from_destination","compare_display_and_target","safe_link"},
    {"unsafe_database_tool_handoff","tools","unparameterized_database_query_request","require_parameterized_read_only_query","db_query"},
    {"indirect_instruction_in_tool_result","injection","untrusted_result_reinterpreted_as_instruction","isolate_result_as_data_only","untrusted_content"},
    {"unvalidated_a2a_message","serialization","malformed_or_oversized_task_message","validate_message_schema_and_size","message_schema"},
    {"unverified_callback_destination","callbacks","callback_to_unapproved_endpoint","require_allowlisted_callback","callback_destination"},
    {"credential_forwarding_across_agents","credentials","credentials_propagated_to_subagents","no_delegated_credentials","credential_forwarding"},
    {"privileged_tool_scope_escalation","tools","subagent_requests_privileged_tool","restrict_tool_scope_to_read_only","tool_scope"},
    {"uncorrelated_agent_event","observability","agent_handoff_without_trace_and_audit","require_correlated_audit_event","audit_trace"}
  };
  return items;
}
std::string list_scenarios() {
  std::string out=R"({"ok":true,"mode":"simulation_only","scenario_kind":"synthetic_multiagent_security","network_access":false,"side_effects":false,"scenarios":[)";
  for(std::size_t i=0;i<scenarios().size();++i) {
    const auto& s=scenarios()[i];
    if(i) out+=',';
    out+="{\"id\":\""+std::string(s.id)+"\",\"category\":\""+std::string(s.category)+"\"}";
  }
  return out+"]}";
}
std::string assess(std::string_view id,std::string_view role) {
  if(role!="reviewer") return deny();
  for(const auto& s:scenarios()) if(s.id==id) {
    return "{\"ok\":true,\"mode\":\"simulation_only\",\"decision\":\"allow_preview\",\"scenario\":\""+std::string(s.id)+"\",\"control\":\""+std::string(s.control)+"\",\"network_access\":false,\"side_effects\":false}";
  }
  return deny();
}
std::string simulate(std::string_view id,std::string_view role) {
  if(role!="reviewer") return deny();
  for(const auto& s:scenarios()) if(s.id==id) {
    const auto d=verify_fixture(negative_fixture(s.failed_gate));
    if(d.accepted || d.failed_gate!=s.failed_gate) return deny();
    return "{\"ok\":true,\"mode\":\"simulation_only\",\"decision\":\"deny\",\"scenario\":\""+std::string(s.id)+"\",\"blocked_gate\":\""+std::string(d.failed_gate)+"\",\"network_access\":false,\"side_effects\":false}";
  }
  return deny();
}
std::string baseline(std::string_view role) {
  if(role!="reviewer" || !verify_fixture(DelegationEvidence{}).accepted) return deny();
  return R"({"ok":true,"mode":"simulation_only","decision":"allow_preview","fixture":"approved_read_only_agent_handoff","actual_a2a_transport_connected":false,"cryptographic_signatures_verified":false,"live_tasks_executed":0,"network_access":false,"side_effects":false})";
}
std::string delegate(std::string_view role,std::string_view tenant_scope,
                     std::string_view session_scope,std::string_view capability,
                     std::string_view approval) {
  DelegationEvidence e;
  e.role=role; e.tenant_scope=tenant_scope; e.session_scope=session_scope;
  e.capability=capability; e.approval=approval;
  if(!verify_fixture(e).accepted) return deny();
  return R"({"ok":true,"mode":"simulation_only","decision":"allow_preview","fixture":"synthetic_read_only_delegation","actual_a2a_transport_connected":false,"network_access":false,"side_effects":false})";
}
std::string report() {
  return R"({"ok":true,"mode":"simulation_only","scenario_count":24,"real_agents_registered":0,"live_tasks_executed":0,"signed_agent_cards_cryptographically_verified":0,"measured_attack_success_rate":null,"network_access":false,"side_effects":false})";
}
} // namespace simulator::a2a_security
