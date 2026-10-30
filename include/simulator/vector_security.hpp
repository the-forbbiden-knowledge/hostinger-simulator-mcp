#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace simulator::vector_security {
struct Scenario {
  std::string_view id;
  std::string_view risk_category;
  std::string_view threat;
  std::string_view required_control;
  std::string_view failed_gate;
};

// FIXTURE EVIDENCE ONLY: a real service MUST derive these fields from
// authenticated server identity, vetted index settings and trusted audit logs.
// None of these values is a cryptographic verification or a live data query.
struct RetrievalEvidence {
  std::string_view role{"reviewer"};
  std::string_view tenant_scope{"same_tenant"};
  std::string_view session_scope{"same_session"};
  std::string_view classification{"public"};
  std::string_view purpose{"search"};
  std::string_view result_kind{"redacted_summary"};
  std::string_view index_name{"approved_fixture_index"};
  std::string_view model_revision{"pinned_fixture_revision"};
  int top_k{3};
  int daily_query_count{1};
  bool access_list_checked{true};
  bool index_isolated{true};
  bool encryption_enabled{true};
  bool provenance_verified{true};
  bool redaction_reviewed{true};
  bool input_data_minimized{true};
  bool suspicious_query_blocked{true};
  bool repeated_probe_blocked{true};
  bool audit_event_recorded{true};
  bool model_metadata_private{true};
  bool vector_export_disabled{true};
  bool raw_chunk_export_disabled{true};
  bool write_requires_approval{true};
};

struct Decision { bool accepted; std::string_view failed_gate; };
Decision verify_fixture(const RetrievalEvidence& e) noexcept;
const std::vector<Scenario>& scenarios();
std::string list_scenarios();
std::string assess(std::string_view scenario, std::string_view role);
std::string simulate(std::string_view scenario, std::string_view role);
std::string baseline(std::string_view role);
std::string query(std::string_view role, std::string_view tenant_scope,
                  std::string_view classification, std::string_view purpose,
                  std::string_view result_kind);
std::string report();
} // namespace simulator::vector_security
