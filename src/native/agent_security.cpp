#include "simulator/agent_security.hpp"
#include "simulator/simulator.hpp"
#include <algorithm>
#include <initializer_list>
#include <string>

namespace simulator::agent_security {
namespace {
bool one_of(std::string_view candidate, std::initializer_list<std::string_view> options) {
  return std::find(options.begin(), options.end(), candidate) != options.end();
}
std::string deny(std::string_view reason) {
  return "{\"ok\":true,\"mode\":\"simulation_only\",\"decision\":\"deny\",\"reason\":\"" +
         simulator::escape_json(reason) + "\",\"side_effects\":false}";
}
} // namespace

const std::vector<Scenario>& scenarios() {
  static const std::vector<Scenario> entries{
    {"agent_surface_inventory", "discovery", "tool_registry_and_contract_review"},
    {"direct_instruction_override", "user_input", "trusted_policy_precedence"},
    {"system_configuration_disclosure", "user_input", "no_sensitive_egress"},
    {"output_filter_bypass", "user_input", "data_classification_at_sink"},
    {"troubleshooting_pretext", "user_input", "per_action_authorization"},
    {"objective_reframing", "user_input", "per_action_authorization"},
    {"multi_turn_escalation", "conversation", "authorization_each_step"},
    {"multi_document_fragmentation", "document_batch", "aggregate_context_untrusted"},
    {"hidden_web_content", "web_page", "untrusted_extracted_content"},
    {"source_code_unicode_tricks", "source_code", "canonicalization_and_integrity_review"},
    {"tool_response_poisoning", "tool_output", "tool_data_never_authorizes"},
    {"shared_knowledge_poisoning", "shared_memory", "reviewed_provenance_and_write_gate"},
    {"predictable_session_id", "session", "opaque_unpredictable_session_tokens"},
    {"cross_session_memory_access", "session", "isolation_and_scope_binding"}
  };
  return entries;
}

std::string list_scenarios() {
  std::string result = "{\"mode\":\"simulation_only\",\"scenario_kind\":\"synthetic_threat_model\",\"scenarios\":[";
  for (const auto& s: scenarios()) {
    if (&s != &scenarios().front()) result += ',';
    result += "{\"id\":\"" + simulator::escape_json(s.id) + "\",\"entry_point\":\"" +
      simulator::escape_json(s.entry_point) + "\",\"required_control\":\"" +
      simulator::escape_json(s.required_control) + "\",\"status\":\"design_case\"}";
  }
  result += "]}";
  return result;
}

std::string evaluate(std::string_view source, std::string_view action,
                     std::string_view role, std::string_view session_scope,
                     std::string_view approval) {
  if (!one_of(source, {"trusted_application", "user_input", "document", "document_batch", "web_page",
                        "source_code", "tool_output", "shared_memory", "private_memory"}) ||
      !one_of(action, {"read_fixture", "execute_tool", "write_shared_memory", "read_private_memory",
                       "reveal_secret", "external_egress"}) ||
      !one_of(role, {"viewer", "operator", "reviewer"}) ||
      !one_of(session_scope, {"same_session", "cross_session"}) ||
      !one_of(approval, {"none", "approved"})) {
    return deny("invalid_policy_input");
  }
  if (action == "reveal_secret" || action == "external_egress") return deny("sensitive_egress_disabled");
  if (session_scope != "same_session") return deny("session_boundary");
  if (action == "read_fixture") {
    return "{\"ok\":true,\"mode\":\"simulation_only\",\"decision\":\"allow_preview\",\"reason\":\"synthetic_read_only\",\"side_effects\":false}";
  }
  if (source != "trusted_application") return deny("untrusted_data_cannot_authorize_actions");
  if (action == "write_shared_memory" && (role != "reviewer" || approval != "approved")) {
    return deny("memory_write_requires_review");
  }
  if ((action == "execute_tool" || action == "read_private_memory") && role == "viewer") {
    return deny("insufficient_role");
  }
  if ((action == "execute_tool" || action == "read_private_memory") && approval != "approved") return deny("approval_required");
  return "{\"ok\":true,\"mode\":\"simulation_only\",\"decision\":\"allow_preview\",\"reason\":\"authorized_preview_only\",\"side_effects\":false}";
}
std::string guarded_call(std::string_view source, std::string_view role,
                         std::string_view session_scope, std::string_view approval,
                         std::string_view tool, std::string_view resource) {
  const auto policy = evaluate(source, "execute_tool", role, session_scope, approval);
  if (policy.find("\"decision\":\"allow_preview\"") == std::string::npos) return policy;
  return simulator::call_tool(tool, resource);
}
} // namespace simulator::agent_security
