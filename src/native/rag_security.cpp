#include "simulator/rag_security.hpp"
#include <string>
#include <array>
namespace simulator::rag_security {
namespace {
std::string deny(){return R"({"ok":false,"decision":"deny","error":"not_available_or_authorized","mode":"simulation_only","network_access":false,"side_effects":false})";}
}
Decision check(const Evidence& e) noexcept {
  if(e.role!="reviewer") return {false,"role"};
  if(e.scope!="approved_fixture") return {false,"scope"};
  if(e.session!="same_session") return {false,"session"};
  if(e.ingestion!="approved_ingestion") return {false,"ingestion"};
  if(e.source!="verified_source") return {false,"source"};
  if(e.connector!="approved_connector") return {false,"connector"};
  if(e.manifest!="protected_manifest") return {false,"manifest"};
  if(e.version!="current_approved") return {false,"version"};
  if(e.change!="reviewed_update") return {false,"change"};
  if(e.history!="diff_reviewed") return {false,"history"};
  if(e.format!="allowlisted_text") return {false,"format"};
  if(e.size!="bounded") return {false,"size"};
  if(e.normalization!="reviewed_normalized") return {false,"normalization"};
  if(e.content!="data_only") return {false,"content"};
  if(e.instruction!="non_authoritative") return {false,"instruction"};
  if(e.chunk!="reviewed_boundaries") return {false,"chunk"};
  if(e.mapping!="bound_source_id") return {false,"mapping"};
  if(e.embed!="pinned_model") return {false,"embed"};
  if(e.diversity!="source_diversity") return {false,"diversity"};
  if(e.ranking!="bounded_fusion") return {false,"ranking"};
  if(e.vector!="verified_index") return {false,"vector"};
  if(e.keyword!="verified_index") return {false,"keyword"};
  if(e.acl!="consistent_before_retrieval") return {false,"acl"};
  if(e.tenant!="same_tenant") return {false,"tenant"};
  if(e.classification!="public") return {false,"classification"};
  if(e.topk!="bounded") return {false,"topk"};
  if(e.rate!="bounded") return {false,"rate"};
  if(e.rerank!="acl_rechecked") return {false,"rerank"};
  if(e.context!="delimited_untrusted") return {false,"context"};
  if(e.citation!="verified_source") return {false,"citation"};
  if(e.output!="redacted_typed") return {false,"output"};
  if(e.tool!="no_implicit_tools") return {false,"tool"};
  if(e.cache!="tenant_bound") return {false,"cache"};
  if(e.audit!="redacted_trace") return {false,"audit"};
  if(e.mutation!="none") return {false,"mutation"};
  if(!e.no_live_access) return {false,"no_live_access"};
  return {true,"none"};
}
Evidence failing_fixture(std::string_view gate) {
  Evidence e;
  if(gate=="role") e.role="viewer";
  if(gate=="scope") e.scope="out_of_scope";
  if(gate=="session") e.session="cross_session";
  if(gate=="ingestion") e.ingestion="unapproved_upload";
  if(gate=="source") e.source="anonymous_source";
  if(gate=="connector") e.connector="external_unreviewed";
  if(gate=="manifest") e.manifest="self_claimed_hash";
  if(gate=="version") e.version="stale_version";
  if(gate=="change") e.change="mass_overwrite";
  if(gate=="history") e.history="gradual_undetected";
  if(gate=="format") e.format="unsafe_object";
  if(gate=="size") e.size="oversized";
  if(gate=="normalization") e.normalization="obfuscated_unicode";
  if(gate=="content") e.content="embedded_html_command";
  if(gate=="instruction") e.instruction="promoted_to_system";
  if(gate=="chunk") e.chunk="poisoned_overlap";
  if(gate=="mapping") e.mapping="spoofed_metadata";
  if(gate=="embed") e.embed="model_mismatch";
  if(gate=="diversity") e.diversity="collision_boost";
  if(gate=="ranking") e.ranking="keyword_stuffing";
  if(gate=="vector") e.vector="tampered_index";
  if(gate=="keyword") e.keyword="tampered_index";
  if(gate=="acl") e.acl="metadata_acl_mismatch";
  if(gate=="tenant") e.tenant="other_tenant";
  if(gate=="classification") e.classification="confidential";
  if(gate=="topk") e.topk="unbounded";
  if(gate=="rate") e.rate="query_flood";
  if(gate=="rerank") e.rerank="acl_bypass";
  if(gate=="context") e.context="elevated_document";
  if(gate=="citation") e.citation="missing_source";
  if(gate=="output") e.output="private_disclosure";
  if(gate=="tool") e.tool="privileged_tool";
  if(gate=="cache") e.cache="shared_cache";
  if(gate=="audit") e.audit="missing_audit";
  if(gate=="mutation") e.mutation="external_write";
  if(gate=="no_live_access") e.no_live_access=false;
  return e;
}
const std::vector<Scenario>& scenarios() {
  static const std::vector<Scenario> cases {
    {"unauthorized_document_ingestion","ingestion","Unapproved documents enter corpus","Require approved ingress identity and workflow","ingestion"},
    {"unverified_document_source","ingestion","Document provenance spoofed","Bind import to approved source identity","source"},
    {"unreviewed_connector","ingestion","Third-party connector imports unreviewed content","Allowlist ingestion connectors","connector"},
    {"hash_without_trust","integrity","Hash of attacker-supplied data treated as endorsement","Match protected immutable allowlist manifest","manifest"},
    {"stale_document_version","integrity","Superseded source used as current authority","Versioned document approval","version"},
    {"rapid_doc_replacements","integrity","Unexpected bulk overwrite of trusted corpus","Controlled update and change review","change"},
    {"gradual_document_poisoning","integrity","Small cumulative edits hide instruction drift","History-aware change-diff and review","history"},
    {"dangerous_upload_type","parsing","Executable or unsupported content reaches parser","Safe parsers with allowed file types","format"},
    {"oversized_ingestion","parsing","Document or chunk exceeds bounded limits","Content and chunk quotas","size"},
    {"obfuscated_payload","normalization","Encoded or invisible characters evade simple filters","Normalize and inspect suspicious content","normalization"},
    {"hidden_html_instruction","content","Embedded markup directs agent actions","Treat all document markup as untrusted data","content"},
    {"ingestion_instruction","content","Retrieved instructions become trusted commands","Separate instructions from retrieved data","instruction"},
    {"poisoned_chunk_overlap","chunking","Attack instructions persist across overlapping chunks","Validate chunk boundaries and metadata","chunk"},
    {"spoofed_chunk_metadata","chunking","Mismatched chunk metadata alters attribution","Immutable chunk-to-source mapping","mapping"},
    {"embedding_model_mismatch","embedding","Query and ingestion model disagree","Pinned model identity and dimension","embed"},
    {"synthetic_embedding_collision","ranking","Artificially broad text dominates similarity matches","Rank diversity and source review","diversity"},
    {"keyword_ranking_manipulation","ranking","Keyword stuffing wins hybrid ranking","Bound and review ranking fusion","ranking"},
    {"tampered_vector_index","storage","Vector record diverges from approved source","Verify index to source mapping","vector"},
    {"tampered_keyword_index","storage","Keyword index diverges from approved source","Verify keyword provenance mapping","keyword"},
    {"metadata_acl_mismatch","storage","ACL differs across metadata and vector store","Consistent pre-retrieval ACL","acl"},
    {"cross_tenant_rag_query","retrieval","Results cross tenant boundary","Scope candidates to server-derived tenant","tenant"},
    {"restricted_document_retrieved","retrieval","Private chunk selected for public request","Classification filter before top-k","classification"},
    {"unbounded_top_k","retrieval","Excessive chunks expose confidential context","Bound candidate and context count","topk"},
    {"high_rate_query_probing","retrieval","Repeated queries infer internal documents","Bound query budget","rate"},
    {"rerank_authorization_bypass","retrieval","Reranking restores unauthorized candidates","Recheck ACL after reranking","rerank"},
    {"retrieval_context_hijack","context","Retrieved content acts as system instructions","Strict data-only context boundaries","context"},
    {"uncorroborated_citation","response","Answer cites unverified or invented origin","Source-bound attribution gate","citation"},
    {"sensitive_answer_leak","response","Output exposes confidential values","Output classification and redaction","output"},
    {"rag_tool_escalation","agent","Retrieved chunk drives privileged tool call","Independent action authorization","tool"},
    {"cross_tenant_cache_leak","caching","Cached answer leaks tenant-scoped result","Cache key binds tenant and role","cache"},
  }; return cases;
}
std::string list(){
  std::string s=R"({"ok":true,"mode":"simulation_only","scenario_count":30,"scenarios":[)";
  bool first=true;
  for(const auto& c:scenarios()){
    if(!first)s+=',';first=false;
    s+="{\"id\":\""+std::string(c.id)+"\",\"domain\":\""+std::string(c.domain)+"\"}";
  }
  return s+"]}";
}
std::string assess(std::string_view id,std::string_view role){
 if(role!="reviewer")return deny();
 for(const auto& c:scenarios())if(c.id==id)
  return "{\"ok\":true,\"id\":\""+std::string(c.id)+"\",\"risk\":\""+std::string(c.risk)+"\",\"gate\":\""+std::string(c.failing_gate)+"\",\"mode\":\"simulation_only\"}";
 return deny();
}
std::string simulate(std::string_view id,std::string_view role){
 if(role!="reviewer")return deny();
 for(const auto& c:scenarios())if(c.id==id){
   auto d=check(failing_fixture(c.failing_gate));
   return "{\"ok\":true,\"id\":\""+std::string(c.id)+"\",\"decision\":\""+(d.allowed?"allow_preview":"no_go")+"\",\"failed_gate\":\""+std::string(d.failed_gate)+"\",\"network_access\":false,\"side_effects\":false}";
 }
 return deny();
}
std::string baseline(std::string_view role){
 if(role!="reviewer")return deny();
 const auto d=check(Evidence{});
 return d.allowed? R"({"ok":true,"decision":"allow_preview","fixture":"approved_read_only","network_access":false,"side_effects":false})" : deny();
}
std::string evaluate(std::string_view role,std::string_view tenant,std::string_view source,std::string_view acl,std::string_view context,std::string_view output){
 Evidence e;
 e.role=role;e.tenant=tenant;e.source=source;e.acl=acl;e.context=context;e.output=output;
 auto d=check(e);
 if(!d.allowed)return deny();
 return R"({"ok":true,"decision":"allow_preview","evidence":"synthetic_only","network_access":false,"side_effects":false})";
}
std::string retrieve(std::string_view role,std::string_view tenant,std::string_view classification,std::string_view purpose){
 // Pretend a metadata store contains both public and restricted rows.
 // Pre-filter by a trusted fixture identity BEFORE any ranking or top-k.
 if(role!="reviewer"||tenant!="same_tenant"||classification!="public"||purpose!="knowledge_preview")return deny();
 struct Row{std::string_view tenant,classification,source,summary;};
 static constexpr std::array<Row,3> rows{{
  {"same_tenant","public","synthetic-guide-001","Sanitized demonstration of safe preview"},
  {"other_tenant","public","synthetic-external-002","UNREACHABLE"},
  {"same_tenant","confidential","synthetic-internal-003","UNREACHABLE"}
 }};
 for(const auto& row:rows){
  if(row.tenant==tenant && row.classification==classification){
   return "{\"ok\":true,\"decision\":\"allow_preview\",\"source\":\""+std::string(row.source)+"\",\"summary\":\""+std::string(row.summary)+"\",\"chunks_returned\":1,\"network_access\":false,\"side_effects\":false}";
  }
 }
 return deny();
}
std::string report(){
 return R"({"ok":true,"mode":"simulation_only","scenario_count":30,"independent_gates":36,"tested_fixture":"bounded_offline","network_access":false,"side_effects":false,"operational_rag_connected":false})";
}
} // namespace simulator::rag_security
