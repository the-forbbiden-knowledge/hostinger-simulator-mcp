#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace simulator::infrastructure_security {
struct Scenario {
  std::string_view id;
  std::string_view category;
  std::string_view risk;
  std::string_view control;
  std::string_view failing_gate;
};

// Defaults describe a safe local test fixture, never a real cloud configuration.
struct PolicyEvidence {
  std::string_view role{"reviewer"};
  std::string_view scope{"approved_fixture"};
  std::string_view tenant_scope{"same_tenant"};
  std::string_view cloud_identity{"least_privilege"};
  std::string_view namespace_scope{"same_namespace"};
  std::string_view endpoint_auth{"private_authenticated"};
  std::string_view storage_policy{"private_scoped"};
  std::string_view credentials{"short_lived"};
  std::string_view secrets{"redacted"};
  std::string_view service_account{"dedicated_no_automount"};
  std::string_view rbac{"minimal_read_only"};
  std::string_view admission{"restricted"};
  std::string_view pod_privilege{"unprivileged"};
  std::string_view host_mount{"none"};
  std::string_view runtime{"sandboxed"};
  std::string_view gpu_isolation{"isolated"};
  std::string_view image_provenance{"approved_digest"};
  std::string_view build_attestation{"verified_fixture"};
  std::string_view egress{"denied"};
  std::string_view audit{"redacted_audited"};
  std::string_view approval{"approved"};
  std::string_view activity{"read_only_preview"};
  std::string_view artifact_origin{"synthetic"};
  std::string_view policy_source{"trusted"};
  bool no_external_access{true};
  bool resource_limits{true};
};
struct Decision { bool allowed_preview; std::string_view failed_gate; };
Decision check_policy(const PolicyEvidence& evidence) noexcept;
const std::vector<Scenario>& scenarios();
std::string list_scenarios();
std::string assess(std::string_view id,std::string_view role);
std::string simulate(std::string_view id,std::string_view role);
std::string baseline(std::string_view role);
std::string evaluate(std::string_view role,std::string_view scope,std::string_view cloud_identity,
                     std::string_view namespace_scope,std::string_view activity,std::string_view approval);
std::string report();
} // namespace simulator::infrastructure_security
