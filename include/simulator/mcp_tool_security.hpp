#pragma once
#include <string>
#include <string_view>
#include <vector>
namespace simulator::mcp_tool_security {
struct Scenario { std::string_view id, domain, risk, control, failing_gate; };
// Trusted fixture values are created by the test harness, never derived from prompts.
struct Evidence {
  std::string_view role{"reviewer"};
  std::string_view approved_scope{"approved_fixture"};
  std::string_view tenant{"same_tenant"};
  std::string_view session{"same_session"};
  std::string_view source{"trusted_application"};
  std::string_view registry{"pinned_registry"};
  std::string_view tool_identity{"qualified_unique"};
  std::string_view description{"reviewed_metadata"};
  std::string_view schema{"strict_schema"};
  std::string_view errors{"minimized_errors"};
  std::string_view discovery{"authorized_only"};
  std::string_view server_identity{"authenticated_server"};
  std::string_view transport{"offline_fixture"};
  std::string_view origin{"verified_origin"};
  std::string_view authn{"server_authenticated"};
  std::string_view authz{"independent_gate"};
  std::string_view capability{"read_only_tool"};
  std::string_view principal{"scoped_principal"};
  std::string_view resource{"fixture_only"};
  std::string_view filesystem{"canonical_allowlist"};
  std::string_view database{"read_only_view"};
  std::string_view egress{"no_network"};
  std::string_view result{"redacted_typed"};
  std::string_view cross_tool{"independent_review"};
  std::string_view approval{"approved"};
  std::string_view ui{"sandboxed_csp"};
  std::string_view bridge{"restricted_bridge"};
  std::string_view rate{"bounded"};
  std::string_view audit{"redacted_correlation"};
  std::string_view mutation{"none"};
  std::string_view timeout{"bounded"};
  std::string_view sandbox{"isolated_process"};
  bool no_live_access{true};
};
struct Decision { bool allowed; std::string_view failed_gate; };
Decision check(const Evidence&) noexcept;
Evidence failing_fixture(std::string_view gate);
const std::vector<Scenario>& scenarios();
std::string list();
std::string assess(std::string_view id, std::string_view role);
std::string simulate(std::string_view id, std::string_view role);
std::string baseline(std::string_view role);
std::string evaluate(std::string_view role, std::string_view tenant, std::string_view source,
                     std::string_view registry, std::string_view mutation, std::string_view approval);
std::string report();
} // namespace simulator::mcp_tool_security
