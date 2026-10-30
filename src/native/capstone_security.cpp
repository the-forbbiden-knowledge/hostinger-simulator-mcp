#include "simulator/capstone_security.hpp"
#include "simulator/simulator.hpp"
#include <algorithm>

namespace simulator::capstone_security {
namespace {
std::string deny() {
  // Same answer for nonexistent scenarios, insufficient permissions, and invalid inputs.
  return R"({"ok":true,"mode":"simulation_only","decision":"deny","reason":"not_authorized_or_unavailable","network_access":false,"side_effects":false})";
}
}

const std::vector<Scenario>& scenarios() {
  static const std::vector<Scenario> items{
    {"public_ai_database_broker", "public_ai_to_database", "unvalidated_model_generated_database_operation", "parameterized_read_only_queries_and_sql_execution_allowlist"},
    {"public_tool_authority", "public_ai_to_tools", "agent_generated_privileged_tool_action", "application_owned_tool_policy_and_per_call_authorization"},
    {"dmz_workload_separation", "public_to_dmz", "public_service_to_internal_identity_trust", "isolated_service_identity_and_default_deny_egress"},
    {"gateway_group_authorization", "dmz_to_remote_gateway", "unauthorized_remote_access_membership", "privileged_group_change_approval_and_audit"},
    {"gateway_session_segmentation", "remote_gateway_to_development", "unscoped_remote_session", "identity_bound_remote_session_and_network_acl"},
    {"development_endpoint_isolation", "development_endpoint", "local_privilege_boundary_failure", "endpoint_hardening_and_privilege_separation"},
    {"developer_credential_boundary", "development_to_source_control", "developer_token_exposure", "scoped_tokens_rotation_and_credential_storage"},
    {"database_service_identity", "development_to_database", "database_credential_reuse", "unique_identity_and_least_privilege_database_roles"},
    {"internal_share_boundary", "database_to_internal_share", "unauthorized_shared_file_disclosure", "per_principal_share_acl_and_access_audit"},
    {"shared_script_provenance", "shared_file_to_automation", "untrusted_shared_script_modification", "signed_artifacts_and_immutable_execution_source"},
    {"module_import_integrity", "automation_to_module", "untrusted_module_resolution", "pinned_module_paths_and_hash_verification"},
    {"scheduled_worker_integrity", "workstation_to_service", "writable_service_binary_path", "protected_install_directory_and_binary_integrity"},
    {"rag_instruction_isolation", "knowledge_to_agent", "retrieved_document_as_privileged_instruction", "data_only_retrieval_and_reviewed_provenance"},
    {"rag_file_boundary", "agent_to_filesystem", "out_of_scope_agent_file_read", "canonical_path_scope_and_server_side_file_allowlist"},
    {"memory_credential_exposure", "process_to_identity", "credential_material_exposed_to_worker", "secret_isolation_and_memory_access_control"},
    {"directory_privilege_chaining", "identity_to_directory", "privileged_identity_reuse_across_zones", "tiered_admin_identities_and_identity_authorization"},
    {"cross_zone_event_correlation", "all_boundaries_to_monitoring", "multi_stage_activity_not_correlated", "stable_trace_ids_and_cross_boundary_detection"}
  };
  return items;
}

std::string list_scenarios() {
  std::string out = R"({"ok":true,"mode":"simulation_only","scenario_kind":"synthetic_cross_zone_guard_inventory","network_access":false,"side_effects":false,"scenarios":[)";
  bool first = true;
  for (const auto& x : scenarios()) {
    if (!first) out += ',';
    first = false;
    out += R"({"id":")" + simulator::escape_json(x.id) + R"(","boundary":")" +
           simulator::escape_json(x.boundary) + R"(","status":"fixture_only"})";
  }
  out += R"(]})";
  return out;
}

std::string assess(std::string_view id, std::string_view role) {
  // Test role is a local fixture, never an authenticated identity.
  if (role != "reviewer") return deny();
  const auto& items = scenarios();
  const auto it = std::find_if(items.begin(), items.end(),
      [id](const Scenario& x) { return x.id == id; });
  if (it == items.end()) return deny();
  return R"({"ok":true,"mode":"simulation_only","decision":"allow_preview","scenario":")" +
     simulator::escape_json(it->id) + R"(","boundary":")" +
     simulator::escape_json(it->boundary) + R"(","risk":")" +
     simulator::escape_json(it->risk) + R"(","required_control":")" +
     simulator::escape_json(it->required_control) +
     R"(","evidence":"synthetic_fixture_only","network_access":false,"side_effects":false})";
}

std::string simulate(std::string_view scenario_id, std::string_view source,
                     std::string_view role, std::string_view session_scope,
                     std::string_view approval) {
  // Every modeled cross-zone escalation is denied, regardless of fixture role.
  // This is a model of desired control decisions, NOT a real gateway/agent/AD test.
  // A trusted, local, read-only health preview is the sole positive-control fixture.
  if (scenario_id != "safe_health_preview" || source != "trusted_application" ||
      (role != "operator" && role != "reviewer") ||
      session_scope != "same_session" || approval != "approved") return deny();
  return R"({"ok":true,"mode":"simulation_only","decision":"allow_preview","scenario":"safe_health_preview","action":"read_only_health","reason":"trusted_local_preview","network_access":false,"side_effects":false})";
}

std::string report() {
  // Coverage is modeled, not measured operational effectiveness.
  return R"({"ok":true,"mode":"simulation_only","report":"cross_zone_control_model","modeled_blocked_paths":)" +
      std::to_string(scenarios().size()) +
      R"(,"real_attacks_executed":0,"real_detection_rate":null,"network_access":false,"side_effects":false})";
}
} // namespace simulator::capstone_security
