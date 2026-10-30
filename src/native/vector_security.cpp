#include "simulator/vector_security.hpp"
#include <string>
#include <string_view>
#include <vector>

namespace simulator::vector_security {
namespace {
std::string deny() {
  return R"({"ok":true,"mode":"simulation_only","decision":"deny","reason":"not_authorized_or_unavailable","network_access":false,"side_effects":false})";
}

RetrievalEvidence negative_fixture(std::string_view gate) {
  RetrievalEvidence e;
  if (gate=="acl") e.access_list_checked=false;
  else if (gate=="tenant_scope") e.tenant_scope="other_tenant";
  else if (gate=="session_scope") e.session_scope="other_session";
  else if (gate=="role") e.role="viewer";
  else if (gate=="vector_export") e.vector_export_disabled=false;
  else if (gate=="raw_chunk_export") e.raw_chunk_export_disabled=false;
  else if (gate=="metadata") e.model_metadata_private=false;
  else if (gate=="model_revision") e.model_revision="mutable_latest";
  else if (gate=="suspicious_query") e.suspicious_query_blocked=false;
  else if (gate=="probe") e.repeated_probe_blocked=false;
  else if (gate=="encryption") e.encryption_enabled=false;
  else if (gate=="index") e.index_name="unknown_index";
  else if (gate=="top_k") e.top_k=200;
  else if (gate=="query_budget") e.daily_query_count=10001;
  else if (gate=="provenance") e.provenance_verified=false;
  else if (gate=="redaction") e.redaction_reviewed=false;
  else if (gate=="index_isolation") e.index_isolated=false;
  else if (gate=="approval") e.write_requires_approval=false;
  else if (gate=="audit") e.audit_event_recorded=false;
  else if (gate=="data_minimization") e.input_data_minimized=false;
  return e;
}

} // namespace
Decision verify_fixture(const RetrievalEvidence& e) noexcept {
  // Evaluation order deliberately consistent across fixtures.
  if (e.role!="reviewer" && e.role!="operator") return {false,"role"};
  if (e.tenant_scope!="same_tenant") return {false,"tenant_scope"};
  if (e.session_scope!="same_session") return {false,"session_scope"};
  if (!e.access_list_checked) return {false,"acl"};
  if (e.classification!="public" && e.classification!="internal") return {false,"classification"};
  if (e.classification=="internal" && e.role!="reviewer") return {false,"role"};
  if (e.purpose!="search") return {false,"purpose"};
  if (e.result_kind!="redacted_summary") return {false,"result_kind"};
  if (e.index_name!="approved_fixture_index") return {false,"index"};
  if (e.model_revision!="pinned_fixture_revision") return {false,"model_revision"};
  if (e.top_k<1 || e.top_k>5) return {false,"top_k"};
  if (e.daily_query_count<1 || e.daily_query_count>100) return {false,"query_budget"};
  if (!e.index_isolated) return {false,"index_isolation"};
  if (!e.encryption_enabled) return {false,"encryption"};
  if (!e.provenance_verified) return {false,"provenance"};
  if (!e.redaction_reviewed) return {false,"redaction"};
  if (!e.input_data_minimized) return {false,"data_minimization"};
  if (!e.suspicious_query_blocked) return {false,"suspicious_query"};
  if (!e.repeated_probe_blocked) return {false,"probe"};
  if (!e.audit_event_recorded) return {false,"audit"};
  if (!e.model_metadata_private) return {false,"metadata"};
  if (!e.vector_export_disabled) return {false,"vector_export"};
  if (!e.raw_chunk_export_disabled) return {false,"raw_chunk_export"};
  if (!e.write_requires_approval) return {false,"approval"};
  return {true,"none"};
}

const std::vector<Scenario>& scenarios() {
  static const std::vector<Scenario> items{
    {"unauthenticated_vector_database","exposure","vector_database_without_authenticated_access","authenticate_before_any_query","acl"},
    {"cross_tenant_vector_read","isolation","vector_search_spans_tenant_boundaries","server_derived_tenant_filter","tenant_scope"},
    {"cross_session_retrieval","isolation","retrieval_from_other_session","bind_retrieval_to_session","session_scope"},
    {"role_based_retrieval_bypass","access","unprivileged_sensitive_query","document_acl_before_similarity","role"},
    {"raw_vector_export","inversion","bulk_embedding_export","disable_raw_vector_export","vector_export"},
    {"raw_chunk_export","inversion","reconstruction_via_full_chunk_dump","disable_unredacted_chunk_export","raw_chunk_export"},
    {"model_dimension_leak","reconnaissance","embedding_dimension_fingerprint_exposure","hide_index_and_model_metadata","metadata"},
    {"model_revision_exposure","reconnaissance","model_configuration_disclosure","pin_model_revision_and_hide_config","model_revision"},
    {"sensitive_attribute_inference","attribute_inference","document_attributes_inferred_from_similarity","restrict_sensitive_attribute_queries","suspicious_query"},
    {"candidate_membership_probe","membership_inference","repeated_presence_absence_tests","limit_sensitive_membership_oracles","probe"},
    {"template_bank_inversion","inversion","synthetic_template_matching_risk","deny_raw_vectors_and_enforce_minimization","vector_export"},
    {"beam_search_inversion","inversion","candidate_reconstruction_from_vectors","block_vector_and_chunk_exports","raw_chunk_export"},
    {"pretrained_decoder_reconstruction","inversion","pretrained_embedding_decoder_exposure","encrypt_and_acl_retrieval_storage","encryption"},
    {"surrogate_model_comparison","inversion","untrusted_candidate_model_queries","approved_index_and_model_revision","index"},
    {"large_topk_exposure","bulk_exposure","excessive_nearest_neighbor_results","hard_cap_top_k","top_k"},
    {"high_query_volume_probe","bulk_exposure","many_similarity_queries","per_principal_query_budget","query_budget"},
    {"unreviewed_chunk_provenance","poisoning","unreviewed_retrieved_chunk","signed_or_verified_ingest_provenance","provenance"},
    {"unredacted_summary","disclosure","sensitive_data_in_retrieval_summary","redacted_summary_only","redaction"},
    {"cross_index_reuse","isolation","unsafe_mixed_index","strict_index_isolation","index_isolation"},
    {"unreviewed_vector_ingest","poisoning","unapproved_mutable_vector_entries","approved_index_writes","approval"},
    {"search_trace_missing","observability","unattributed_similarity_access","structured_query_audit_event","audit"},
    {"unminimized_secret_ingest","data_minimization","sensitive_secret_ingested_into_embeddings","minimize_sensitive_input_before_embedding","data_minimization"},
  };
  return items;
}
std::string list_scenarios() {
  std::string out=R"({"ok":true,"mode":"simulation_only","scenario_kind":"synthetic_vector_privacy","network_access":false,"side_effects":false,"scenarios":[)";
  for (std::size_t i=0;i<scenarios().size();++i) {
    const auto& sc=scenarios()[i];
    if(i)out+=',';
    out+="{\"id\":\""+std::string(sc.id)+"\",\"category\":\""+std::string(sc.risk_category)+"\"}";
  }
  return out+"]}";
}
std::string assess(std::string_view id, std::string_view role) {
  if(role!="reviewer")return deny();
  for(const auto& sc:scenarios())if(sc.id==id) {
    return "{\"ok\":true,\"mode\":\"simulation_only\",\"decision\":\"allow_preview\",\"scenario\":\""+std::string(sc.id)+"\",\"control\":\""+std::string(sc.required_control)+"\",\"network_access\":false,\"side_effects\":false}";
  }
  return deny();
}
std::string simulate(std::string_view id, std::string_view role) {
  if(role!="reviewer") return deny();
  for(const auto& sc:scenarios())if(sc.id==id){
    const auto outcome=verify_fixture(negative_fixture(sc.failed_gate));
    if(outcome.accepted || outcome.failed_gate!=sc.failed_gate) return deny();
    return "{\"ok\":true,\"mode\":\"simulation_only\",\"decision\":\"deny\",\"scenario\":\""+std::string(sc.id)+"\",\"blocked_gate\":\""+std::string(outcome.failed_gate)+"\",\"network_access\":false,\"side_effects\":false}";
  }
  return deny();
}
std::string baseline(std::string_view role) {
  if(role!="reviewer")return deny();
  const auto decision=verify_fixture(RetrievalEvidence{});
  if(!decision.accepted)return deny();
  return R"({"ok":true,"mode":"simulation_only","decision":"allow_preview","fixture":"redacted_public_summary","raw_vectors_returned":0,"raw_chunks_returned":0,"actual_vector_database_connected":false,"network_access":false,"side_effects":false})";
}
std::string query(std::string_view role, std::string_view tenant_scope,
                  std::string_view classification, std::string_view purpose,
                  std::string_view result_kind) {
  RetrievalEvidence e;
  e.role=role; e.tenant_scope=tenant_scope; e.classification=classification;
  e.purpose=purpose;e.result_kind=result_kind;
  if(!verify_fixture(e).accepted)return deny();
  return R"({"ok":true,"mode":"simulation_only","decision":"allow_preview","result":"synthetic_redacted_summary","raw_vectors_returned":0,"raw_chunks_returned":0,"actual_vector_database_connected":false,"network_access":false,"side_effects":false})";
}
std::string report() {
  return R"({"ok":true,"mode":"simulation_only","scenario_count":22,"live_queries":0,"vectors_exported":0,"inversion_attempts_executed":0,"real_inference_risk_measured":null,"actual_detection_rate":null,"network_access":false,"side_effects":false})";
}
} // namespace simulator::vector_security

