#pragma once
#include <string>
#include <string_view>
#include <vector>
namespace simulator::a2a_security {
struct Scenario {
  std::string_view id;
  std::string_view category;
  std::string_view threat;
  std::string_view control;
  std::string_view failed_gate;
};
// Synthetic evidence only. Caller-controlled CLI arguments are test inputs, NEVER
// trusted identity or verified transport properties in a live deployment.
struct DelegationEvidence {
  std::string_view role{"reviewer"};
  std::string_view tenant_scope{"same_tenant"};
  std::string_view session_scope{"same_session"};
  std::string_view capability{"read_only"};
  std::string_view approval{"approved"};
  bool registry_auth{true};
  bool card_signature{true};
  bool card_identity{true};
  bool endpoint_origin{true};
  bool transport_tls{true};
  bool capability_allowlist{true};
  bool least_privilege{true};
  bool peer_auth{true};
  bool hop_limit{true};
  bool quorum{true};
  bool task_owner{true};
  bool replay{true};
  bool workflow_integrity{true};
  bool artifact_integrity{true};
  bool safe_link{true};
  bool db_query{true};
  bool untrusted_content{true};
  bool message_schema{true};
  bool callback_destination{true};
  bool credential_forwarding{true};
  bool tool_scope{true};
  bool audit_trace{true};
  int handoff_depth{1};
  int message_bytes{512};
};
struct Decision { bool accepted; std::string_view failed_gate; };
Decision verify_fixture(const DelegationEvidence& evidence) noexcept;
const std::vector<Scenario>& scenarios();
std::string list_scenarios();
std::string assess(std::string_view id, std::string_view role);
std::string simulate(std::string_view id, std::string_view role);
std::string baseline(std::string_view role);
std::string delegate(std::string_view role, std::string_view tenant_scope,
                     std::string_view session_scope, std::string_view capability,
                     std::string_view approval);
std::string report();
} // namespace simulator::a2a_security
