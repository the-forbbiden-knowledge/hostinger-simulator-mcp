#include "simulator/ai_assurance.hpp"
#include <array>
#include <string>
#include <vector>
#include <algorithm>
namespace simulator::ai_assurance {
namespace {
std::string deny(){return R"({"ok":false,"decision":"deny","error":"not_available_or_authorized","mode":"simulation_only","network_access":false,"side_effects":false})";}
constexpr std::array<std::string_view,24> gate_names{{"mission","scope","evidence","confidence","autonomy","authority","memory","knowledge","recovery","fanout","tenant","privacy","fairness","safety","transparency","telemetry","response","consumers","volume","financial","regulatory","rollback","owner","verification"}};
constexpr std::array<std::string_view,24> allowed_values{{"approved","approved_fixture","corroborated","verified","approved_read_only","trusted_application","reviewed_write","current_version","state_rechecked","bounded","same_tenant","redacted","reviewed","reviewed","attributable","redacted_trace","assigned","documented","bounded","unknown_not_invented","reviewed","documented","assigned","independently_tested"}};
}

const std::vector<Scenario>& scenarios(){static const std::vector<Scenario> s{
  {"unapproved_mission","governance","Mission was not approved","Require an explicit authorized test mission","mission"},
  {"out_of_scope_target","scope","Target is outside the approved environment","Enforce a bounded scope","scope"},
  {"unverified_observation","evidence","Uncorroborated claim is promoted into a fact","Require attributable corroborated evidence","evidence"},
  {"ambiguous_result","evidence","Model output is incorrectly treated as confirmed fact","Distinguish observation hypothesis and verification","confidence"},
  {"unauthorized_autonomy","autonomy","Agent takes a consequential step without approval","Require human approval for sensitive actions","autonomy"},
  {"tool_result_command","autonomy","Untrusted tool response becomes instruction authority","Bind instruction authority to trusted application","authority"},
  {"persistent_memory_change","persistence","Long-term agent memory changed without review","Versioned approved memory writes","memory"},
  {"stale_corpus_influence","persistence","Superseded knowledge influences behavior","Validate data provenance and recency","knowledge"},
  {"deployment_restart_persistence","persistence","Stale unsafe state survives a restart","Invalidate affected versions and replay state checks","recovery"},
  {"unsafe_bulk_automation","autonomy","Unbounded automated action fanout","Set limits and explicit escalation","fanout"},
  {"cross_tenant_outcome","privacy","Result exposes another tenant","Server-owned tenant isolation","tenant"},
  {"private_output","privacy","Sensitive data appears in output","Minimize and redact outputs","privacy"},
  {"unreviewed_disparity","fairness","Potential subgroup outcome disparity goes unreviewed","Require a documented outcome review","fairness"},
  {"unsafe_advice","safety","Potentially harmful result is delivered unreviewed","Gate high-risk outputs","safety"},
  {"opaque_decision","transparency","Decision has no usable provenance","Require attributable explanation and trace","transparency"},
  {"no_telemetry","operations","Action has no redacted audit trail","Generate structured redacted evidence","telemetry"},
  {"ignored_alert","operations","Escalation path is absent","Route alerts to a documented owner","response"},
  {"unknown_consumers","impact","Downstream consumers of generated decision are unknown","Inventory consumers before risk acceptance","consumers"},
  {"unknown_volume","impact","Transaction amplification cannot be bounded","Record bounded synthetic scale and rate","volume"},
  {"unsupported_financial_claim","impact","Fictitious loss estimate is treated as actual","Use evidence-backed ranges or mark unknown","financial"},
  {"no_regulatory_context","impact","Potential compliance consequences are unreviewed","Record jurisdiction and reviewer","regulatory"},
  {"no_recovery_plan","impact","No recovery or rollback plan exists","Require owner and rollback criterion","rollback"},
  {"missing_risk_owner","governance","Finding has no accountable owner","Assign decision owner","owner"},
  {"premature_risk_closure","governance","Risk closed without control verification","Require independent validation before closure","verification"},
}; return s;}
Evidence baseline_fixture(){Evidence e;e.gates.reserve(gate_names.size());for(std::size_t i=0;i<gate_names.size();++i)e.gates.push_back({gate_names[i],allowed_values[i]});return e;}
Evidence bad_fixture(std::string_view gate){auto e=baseline_fixture();
if(gate=="mission") for(auto& value:e.gates) if(value.name=="mission") value.value="unapproved";
if(gate=="scope") for(auto& value:e.gates) if(value.name=="scope") value.value="out_of_scope";
if(gate=="evidence") for(auto& value:e.gates) if(value.name=="evidence") value.value="unverified";
if(gate=="confidence") for(auto& value:e.gates) if(value.name=="confidence") value.value="assumed_fact";
if(gate=="autonomy") for(auto& value:e.gates) if(value.name=="autonomy") value.value="unapproved_action";
if(gate=="authority") for(auto& value:e.gates) if(value.name=="authority") value.value="tool_output";
if(gate=="memory") for(auto& value:e.gates) if(value.name=="memory") value.value="poisoned_memory";
if(gate=="knowledge") for(auto& value:e.gates) if(value.name=="knowledge") value.value="stale_corpus";
if(gate=="recovery") for(auto& value:e.gates) if(value.name=="recovery") value.value="unchecked_restart";
if(gate=="fanout") for(auto& value:e.gates) if(value.name=="fanout") value.value="unbounded";
if(gate=="tenant") for(auto& value:e.gates) if(value.name=="tenant") value.value="other_tenant";
if(gate=="privacy") for(auto& value:e.gates) if(value.name=="privacy") value.value="raw_private";
if(gate=="fairness") for(auto& value:e.gates) if(value.name=="fairness") value.value="unreviewed";
if(gate=="safety") for(auto& value:e.gates) if(value.name=="safety") value.value="unsafe";
if(gate=="transparency") for(auto& value:e.gates) if(value.name=="transparency") value.value="unexplained";
if(gate=="telemetry") for(auto& value:e.gates) if(value.name=="telemetry") value.value="absent";
if(gate=="response") for(auto& value:e.gates) if(value.name=="response") value.value="unassigned";
if(gate=="consumers") for(auto& value:e.gates) if(value.name=="consumers") value.value="unknown";
if(gate=="volume") for(auto& value:e.gates) if(value.name=="volume") value.value="unbounded";
if(gate=="financial") for(auto& value:e.gates) if(value.name=="financial") value.value="invented";
if(gate=="regulatory") for(auto& value:e.gates) if(value.name=="regulatory") value.value="unreviewed";
if(gate=="rollback") for(auto& value:e.gates) if(value.name=="rollback") value.value="absent";
if(gate=="owner") for(auto& value:e.gates) if(value.name=="owner") value.value="unassigned";
if(gate=="verification") for(auto& value:e.gates) if(value.name=="verification") value.value="untested";
if(gate=="no_live_access") { e.offline=false; }
return e;}
Decision check(const Evidence& e) noexcept {if(e.role!="reviewer")return {false,"role"};if(e.scope!="approved_fixture")return {false,"scope"};if(e.session!="same_session")return {false,"session"};if(!e.offline)return {false,"no_live_access"};if(e.gates.size()!=gate_names.size())return {false,"evidence_shape"};for(std::size_t i=0;i<gate_names.size();++i){if(e.gates[i].name!=gate_names[i]||e.gates[i].value!=allowed_values[i])return {false,gate_names[i]};}return {true,"none"};}
std::string list(){std::string o=R"({"ok":true,"mode":"simulation_only","network_access":false,"scenarios":[)";bool first=true;for(const auto& c:scenarios()){if(!first)o+=",";first=false;o+="{\"id\":\""+std::string(c.id)+"\",\"domain\":\""+std::string(c.domain)+"\",\"control\":\""+std::string(c.control)+"\"}";}return o+"]}";}
std::string report(){return R"({"ok":true,"mode":"simulation_only","scenario_count":24,"policy_gates":24,"actual_exploits":0,"verified_business_loss":null,"network_access":false,"side_effects":false,"framework_mapping":"descriptive_only"})";}
std::string baseline(std::string_view role){if(role!="reviewer")return deny();return check(baseline_fixture()).allowed? R"({"ok":true,"decision":"allow_preview","scope":"approved_fixture","mode":"simulation_only","network_access":false,"side_effects":false})":deny();}
std::string assess(std::string_view id,std::string_view role){if(role!="reviewer")return deny();for(const auto& c:scenarios())if(c.id==id){return R"({"ok":true,"decision":"review_required","evidence":"synthetic_only","mode":"simulation_only","network_access":false,"side_effects":false})";}return deny();}
std::string simulate(std::string_view id,std::string_view role){if(role!="reviewer")return deny();for(const auto& c:scenarios())if(c.id==id){auto d=check(bad_fixture(c.failing_gate));if(d.allowed)return deny();return std::string(R"({"ok":true,"decision":"no_go","failed_gate":")")+std::string(d.failed_gate)+R"(","mode":"simulation_only","network_access":false,"side_effects":false})";}return deny();}
std::string decide(std::string_view role,std::string_view scope,std::string_view evidence,std::string_view approval,std::string_view telemetry){if(role!="reviewer"||scope!="approved_fixture"||evidence!="corroborated"||approval!="approved_read_only"||telemetry!="redacted_trace")return deny();return R"({"ok":true,"decision":"allow_preview","confidence":"synthetic_only","mode":"simulation_only","network_access":false,"side_effects":false})";}
std::string impact(std::string_view role,std::string_view scope,std::string_view volume,std::string_view consumer,std::string_view financial){if(role!="reviewer"||scope!="approved_fixture"||volume!="bounded"||consumer!="documented"||financial!="unknown_not_invented")return deny();return R"({"ok":true,"decision":"review_required","business_impact":"unverified","estimated_loss":null,"downstream_transactions":"synthetic_bounded","mode":"simulation_only","network_access":false,"side_effects":false})";}
} // namespace simulator::ai_assurance
