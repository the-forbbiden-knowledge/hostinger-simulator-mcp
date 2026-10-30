#include "simulator/recon_security.hpp"
#include "simulator/simulator.hpp"
#include <algorithm>
#include <string>

namespace simulator::recon_security {
namespace {
std::string denied() {
  // Identical denial for unknown and unauthorized IDs: no existence oracle.
  return R"({"ok":true,"mode":"simulation_only","decision":"deny","reason":"not_authorized_or_unavailable","side_effects":false})";
}
} // namespace

const std::vector<Scenario>& scenarios() {
  static const std::vector<Scenario> registry{
      {"public_surface_inventory", "ui_gateway", "unauthorized_service_discovery", "scoped_public_inventory", false},
      {"response_header_fingerprint", "api_gateway", "backend_identity_leak", "minimal_headers", true},
      {"health_endpoint_verbosity", "api_gateway", "internal_topology_leak", "minimal_health_response", true},
      {"client_bundle_configuration", "ui", "debug_or_internal_endpoint_exposure", "public_asset_review", true},
      {"api_status_discrepancy", "api_gateway", "endpoint_existence_oracle", "uniform_auth_boundary", true},
      {"error_response_metadata", "api_gateway", "stack_trace_and_version_exposure", "safe_error_contract", true},
      {"request_rate_behavior", "api_gateway", "bulk_enumeration", "rate_limit_and_alert", true},
      {"model_fingerprint_metadata", "model", "model_and_provider_disclosure", "model_metadata_minimization", true},
      {"model_behavioral_inference", "inference", "capability_profile_inference", "query_budget_and_anomaly_correlation", true},
      {"orchestrator_framework_hints", "orchestration", "framework_fingerprint", "generic_public_errors", true},
      {"mcp_tool_schema_visibility", "agent_tools", "capability_disclosure", "principal_scoped_tool_listing", true},
      {"mcp_permission_boundary", "agent_tools", "privilege_boundary_probing", "server_side_authorization", true},
      {"a2a_agent_metadata", "external_integrations", "agent_relationship_disclosure", "authenticated_capability_discovery", true},
      {"rag_source_attribution", "retrieval", "private_document_name_leak", "source_acl_redaction", true},
      {"rag_chunk_boundary", "retrieval", "retrieval_structure_inference", "tenant_bound_retrieval", true},
      {"rag_embedding_metadata", "retrieval", "embedding_backend_leak", "metadata_minimization", true},
      {"knowledge_source_provenance", "retrieval", "untrusted_source_authority", "reviewed_provenance_and_content_isolation", true},
      {"recon_pattern_detection", "monitoring", "keyword_only_detection_gap", "behavioral_correlation", true},
      {"cross_session_activity", "monitoring", "unlinked_suspicious_sessions", "principal_and_session_correlation", true},
      {"canary_response_integrity", "monitoring", "deceptive_or_honeytoken_artifact", "signed_fixture_labels_and_canary_alerts", true}
  };
  return registry;
}

std::string list_scenarios() {
  std::string result = R"({"mode":"simulation_only","network_access":false,"scenario_kind":"synthetic_exposure_inventory","scenarios":[)";
  bool first = true;
  for (const auto& s : scenarios()) {
    if (!first) result += ',';
    first = false;
    result += R"({"id":")" + simulator::escape_json(s.id) + R"(","layer":")" +
              simulator::escape_json(s.layer) + R"(","status":"fixture_only"})";
  }
  result += R"(]})";
  return result;
}

std::string inspect(std::string_view scenario_id, std::string_view role) {
  // Test role comes from a local fixture. Never trust a client-supplied role in an API.
  if (role != "viewer" && role != "operator" && role != "reviewer") return denied();
  const auto& registry = scenarios();
  const auto item = std::find_if(registry.begin(), registry.end(),
     [scenario_id](const Scenario& s){ return s.id == scenario_id; });
  if (item == registry.end() || (item->restricted && role != "reviewer")) return denied();
  return R"({"ok":true,"mode":"simulation_only","decision":"allow_preview","scenario":")" +
         simulator::escape_json(item->id) + R"(","layer":")" +
         simulator::escape_json(item->layer) + R"(","risk":")" +
         simulator::escape_json(item->risk) + R"(","required_control":")" +
         simulator::escape_json(item->required_control) +
         R"(","evidence":"synthetic_fixture_only","network_access":false,"side_effects":false})";
}
} // namespace simulator::recon_security
