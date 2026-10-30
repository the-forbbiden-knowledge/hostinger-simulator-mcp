#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace simulator::threat_model {

// Evidence state is never derived from a caller's claim of having a privileged role.
enum class EvidenceState { observed, hypothesis, validated, invalidated };
enum class EvidenceAction { hypothesize, corroborate, refute };

// A hypothesis can become validated only with corroborated and attributable evidence.
// An invalidated assumption stays invalidated: a fresh ID and evidence are required.
EvidenceState advance(EvidenceState current, EvidenceAction action,
                      bool evidence_is_attributable, bool corroborated) noexcept;
std::string_view state_name(EvidenceState state) noexcept;

struct Scenario {
  std::string_view id;
  std::string_view category;
  std::string_view risk;
  std::string_view control;
  std::string_view failing_gate;
};

// Synthetic metadata, not authorization or observations about an actual network.
struct PlanEvidence {
  std::string_view role{"reviewer"};
  std::string_view scope{"approved_fixture"};
  std::string_view tenant_scope{"same_tenant"};
  std::string_view evidence_state{"validated"};
  std::string_view confidence{"high"};
  std::string_view engagement_window{"approved_window"};
  std::string_view approval{"approved"};
  std::string_view activity{"read_only_preview"};
  bool provenance{true};
  bool independent_corroboration{true};
  bool trust_boundaries_mapped{true};
  bool crown_jewel_classified{true};
  bool control_owner_assigned{true};
  bool risk_review_complete{true};
  bool data_minimized{true};
  bool report_required_events{true};
  bool audit_log_enabled{true};
  bool negative_findings_recorded{true};
  bool owner_confirmed{true};
  bool no_destructive_actions{true};
  bool no_external_access{true};
  int path_hops{2};
  int remaining_minutes{120};
};
struct Decision { bool allowed_preview; std::string_view failed_gate; };

Decision check_plan(const PlanEvidence& evidence) noexcept;
const std::vector<Scenario>& scenarios();
std::string list_scenarios();
std::string assumptions();
std::string zones();
std::string assess(std::string_view id,std::string_view role);
std::string simulate(std::string_view id,std::string_view role);
std::string baseline(std::string_view role);
std::string plan(std::string_view role,std::string_view scope,
                 std::string_view evidence_state,std::string_view window,
                 std::string_view approval);
std::string brief(std::string_view role);
std::string report();
} // namespace simulator::threat_model
