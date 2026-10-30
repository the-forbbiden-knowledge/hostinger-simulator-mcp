#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace simulator::supplychain_security {
struct Scenario {
  std::string_view id;
  std::string_view layer;
  std::string_view threat;
  std::string_view required_control;
  std::string_view failed_gate;
};

// Synthetic test evidence. These fields MUST NOT be populated by untrusted
// clients as proof of authorization. A future intake service must derive each
// value independently from signed manifests, hashing, registry and review logs.
struct ArtifactEvidence {
  std::string_view component_kind{"model"};
  std::string_view source_registry{"registry.example.invalid"};
  std::string_view namespace_name{"simulator/approved"};
  std::string_view pinned_version{"1.0.0"};
  std::string_view expected_sha256{"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"};
  std::string_view observed_sha256{"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"};
  std::string_view artifact_format{"safetensors"};
  bool signature_verified{true};
  bool provenance_verified{true};
  bool license_approved{true};
  bool inventory_recorded{true};
  bool isolated_review{true};
  bool behavior_reviewed{true};
  bool human_approved{true};
};

struct Decision {
  bool accepted;
  std::string_view failed_gate;
};

Decision verify_fixture(const ArtifactEvidence& evidence) noexcept;
const std::vector<Scenario>& scenarios();
std::string list_scenarios();
std::string assess(std::string_view id, std::string_view role);
std::string simulate(std::string_view id, std::string_view role);
std::string baseline(std::string_view role);
std::string report();
} // namespace simulator::supplychain_security
