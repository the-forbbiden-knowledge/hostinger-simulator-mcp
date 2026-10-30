#pragma once
#include <string>
#include <string_view>
#include <vector>
namespace simulator::rag_security {
struct Scenario { std::string_view id, domain, risk, control, failing_gate; };
// Metadata below is a trusted offline fixture, NOT user authentication or a real scan.
struct Evidence {
  std::string_view role{"reviewer"};
  std::string_view scope{"approved_fixture"};
  std::string_view session{"same_session"};
  std::string_view ingestion{"approved_ingestion"};
  std::string_view source{"verified_source"};
  std::string_view connector{"approved_connector"};
  std::string_view manifest{"protected_manifest"};
  std::string_view version{"current_approved"};
  std::string_view change{"reviewed_update"};
  std::string_view history{"diff_reviewed"};
  std::string_view format{"allowlisted_text"};
  std::string_view size{"bounded"};
  std::string_view normalization{"reviewed_normalized"};
  std::string_view content{"data_only"};
  std::string_view instruction{"non_authoritative"};
  std::string_view chunk{"reviewed_boundaries"};
  std::string_view mapping{"bound_source_id"};
  std::string_view embed{"pinned_model"};
  std::string_view diversity{"source_diversity"};
  std::string_view ranking{"bounded_fusion"};
  std::string_view vector{"verified_index"};
  std::string_view keyword{"verified_index"};
  std::string_view acl{"consistent_before_retrieval"};
  std::string_view tenant{"same_tenant"};
  std::string_view classification{"public"};
  std::string_view topk{"bounded"};
  std::string_view rate{"bounded"};
  std::string_view rerank{"acl_rechecked"};
  std::string_view context{"delimited_untrusted"};
  std::string_view citation{"verified_source"};
  std::string_view output{"redacted_typed"};
  std::string_view tool{"no_implicit_tools"};
  std::string_view cache{"tenant_bound"};
  std::string_view audit{"redacted_trace"};
  std::string_view mutation{"none"};
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
std::string evaluate(std::string_view role,std::string_view tenant,std::string_view source,std::string_view acl,std::string_view context,std::string_view output);
// A bounded, static, non-generative retrieval fixture, never a database lookup.
std::string retrieve(std::string_view role,std::string_view tenant,std::string_view classification,std::string_view purpose);
std::string report();
} // namespace simulator::rag_security
