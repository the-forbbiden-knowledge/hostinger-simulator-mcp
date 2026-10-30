#include "simulator/mcp_tool_security.hpp"
#include <string>
namespace simulator::mcp_tool_security {
namespace {
std::string deny() { return R"({"ok":false,"decision":"deny","error":"not_available_or_authorized","mode":"simulation_only","network_access":false,"side_effects":false})"; }
}
Decision check(const Evidence& e) noexcept {
  if(e.role!="reviewer") return {false,"role"};
  if(e.approved_scope!="approved_fixture") return {false,"approved_scope"};
  if(e.tenant!="same_tenant") return {false,"tenant"};
  if(e.session!="same_session") return {false,"session"};
  if(e.source!="trusted_application") return {false,"source"};
  if(e.registry!="pinned_registry") return {false,"registry"};
  if(e.tool_identity!="qualified_unique") return {false,"tool_identity"};
  if(e.description!="reviewed_metadata") return {false,"description"};
  if(e.schema!="strict_schema") return {false,"schema"};
  if(e.errors!="minimized_errors") return {false,"errors"};
  if(e.discovery!="authorized_only") return {false,"discovery"};
  if(e.server_identity!="authenticated_server") return {false,"server_identity"};
  if(e.transport!="offline_fixture") return {false,"transport"};
  if(e.origin!="verified_origin") return {false,"origin"};
  if(e.authn!="server_authenticated") return {false,"authn"};
  if(e.authz!="independent_gate") return {false,"authz"};
  if(e.capability!="read_only_tool") return {false,"capability"};
  if(e.principal!="scoped_principal") return {false,"principal"};
  if(e.resource!="fixture_only") return {false,"resource"};
  if(e.filesystem!="canonical_allowlist") return {false,"filesystem"};
  if(e.database!="read_only_view") return {false,"database"};
  if(e.egress!="no_network") return {false,"egress"};
  if(e.result!="redacted_typed") return {false,"result"};
  if(e.cross_tool!="independent_review") return {false,"cross_tool"};
  if(e.approval!="approved") return {false,"approval"};
  if(e.ui!="sandboxed_csp") return {false,"ui"};
  if(e.bridge!="restricted_bridge") return {false,"bridge"};
  if(e.rate!="bounded") return {false,"rate"};
  if(e.audit!="redacted_correlation") return {false,"audit"};
  if(e.mutation!="none") return {false,"mutation"};
  if(e.timeout!="bounded") return {false,"timeout"};
  if(e.sandbox!="isolated_process") return {false,"sandbox"};
  if(!e.no_live_access) return {false,"no_live_access"};
  return {true,"none"};
}
Evidence failing_fixture(std::string_view gate) {
  Evidence e;
  if(gate=="role") e.role="untrusted";
  if(gate=="approved_scope") e.approved_scope="out_of_scope";
  if(gate=="tenant") e.tenant="other_tenant";
  if(gate=="session") e.session="cross_session";
  if(gate=="source") e.source="tool_result";
  if(gate=="registry") e.registry="unverified_registry";
  if(gate=="tool_identity") e.tool_identity="shadowed_name";
  if(gate=="description") e.description="poisoned_description";
  if(gate=="schema") e.schema="unbounded_schema";
  if(gate=="errors") e.errors="schema_leak";
  if(gate=="discovery") e.discovery="broad_enumeration";
  if(gate=="server_identity") e.server_identity="spoofed_server";
  if(gate=="transport") e.transport="legacy_unverified_sse";
  if(gate=="origin") e.origin="untrusted_origin";
  if(gate=="authn") e.authn="client_claimed";
  if(gate=="authz") e.authz="model_approved";
  if(gate=="capability") e.capability="admin_tool";
  if(gate=="principal") e.principal="service_owner";
  if(gate=="resource") e.resource="private_source";
  if(gate=="filesystem") e.filesystem="traversal_path";
  if(gate=="database") e.database="schema_owner";
  if(gate=="egress") e.egress="external_destination";
  if(gate=="result") e.result="secret_bearing";
  if(gate=="cross_tool") e.cross_tool="implicit_chain";
  if(gate=="approval") e.approval="missing";
  if(gate=="ui") e.ui="untrusted_html";
  if(gate=="bridge") e.bridge="credential_capture";
  if(gate=="rate") e.rate="unbounded";
  if(gate=="audit") e.audit="missing_audit";
  if(gate=="mutation") e.mutation="production_write";
  if(gate=="timeout") e.timeout="unbounded";
  if(gate=="sandbox") e.sandbox="shared_host";
  if(gate=="no_live_access") e.no_live_access=false;
  return e;
}
const std::vector<Scenario>& scenarios() {
  static const std::vector<Scenario> cases {
    {"unauthorized_tool_enumeration","discovery","Unscoped capabilities disclosed","Scope tools/list to authorized principals","discovery"},
    {"error_schema_overexposure","discovery","Error reveals protected schema","Minimize protected error information","errors"},
    {"cross_server_name_collision","registry","Shadowed tool name redirects call","Server-qualified unique identity","tool_identity"},
    {"registry_origin_unverified","registry","Untrusted tool registry accepted","Pinned registry with review","registry"},
    {"description_instruction_poisoning","metadata","Tool description treated as authority","Metadata is data, signed review required","description"},
    {"unbounded_tool_schema","metadata","Loose schema permits unexpected arguments","Strict input schema and length limits","schema"},
    {"tool_result_instruction","metadata","Tool result attempts to authorize action","Use trusted application provenance only","source"},
    {"spoofed_mcp_server","identity","Tool caller routes to spoofed server","Authenticate and bind server identity","server_identity"},
    {"cross_tenant_tool_access","identity","Tool reaches another tenant","Server-enforced tenant scope","tenant"},
    {"cross_session_tool_access","identity","Tool accesses unrelated session","Independent session binding","session"},
    {"untrusted_login_claim","identity","Client supplies authoritative role","Server-derived authentication only","authn"},
    {"model_authorizes_tool","identity","LLM output becomes authorization","Independent trusted authorization gate","authz"},
    {"administrator_capability_escalation","privilege","Preview requests admin action","Read-only approved capability list","capability"},
    {"backend_owner_credentials","privilege","Tool executes with data owner role","Use scoped non-owner principal","principal"},
    {"unscoped_resource_selector","resource","Tool targets nonfixture resource","Limit to approved synthetic fixture","resource"},
    {"filesystem_directory_escape","filesystem","Path escapes allowed fixture","Canonical path and explicit allowlist","filesystem"},
    {"database_owner_queries","database","Query principal exceeds required schema","Read-only scoped database view","database"},
    {"unexpected_external_egress","network","Tool result escapes to external service","No network calls under simulation","egress"},
    {"credential_in_tool_output","result","Secret appears in result body","Typed redacted results","result"},
    {"implicit_multi_tool_chain","chaining","Privilege accumulation across tools","Authorize each step independently","cross_tool"},
    {"sensitive_action_without_approval","approval","Sensitive tool operation is not confirmed","Independent approval for permitted action","approval"},
    {"unsafe_mcp_app_html","mcp_apps","Untrusted UI rendered without isolation","Sandboxed UI and strict CSP","ui"},
    {"unrestricted_app_bridge","mcp_apps","Embedded UI calls privileged tool","Restrict AppBridge capabilities","bridge"},
    {"unbounded_tool_invocations","availability","Model can repeatedly call tool","Bounded calls and rate limits","rate"},
    {"missing_tool_audit","telemetry","Tool action lacks trace attribution","Redacted correlation and audit","audit"},
    {"unauthorized_production_mutation","mutation","Preview changes live service state","No mutation under offline fixture","mutation"},
    {"unbounded_tool_timeout","resilience","Tool worker can hang indefinitely","Enforce bounded worker timeout","timeout"},
    {"shared_host_native_worker","resilience","Native worker not isolated","Run native tool out-of-process","sandbox"},
    {"out_of_scope_target","governance","Requested action outside approved scope","Approved synthetic-only scope","approved_scope"},
    {"untrusted_operator","identity","Unknown role obtains tool access","Role enforced by service","role"},
    {"legacy_transport_unverified","transport","Legacy SSE transport accepted without review","Offline fixture, future explicit version negotiation","transport"},
    {"untrusted_http_origin","transport","Request origin not verified","Validate Origin and bind to transport policy","origin"},
  };
  return cases;
}
std::string list() {
  std::string s=R"({"ok":true,"mode":"simulation_only","scenario_kind":"synthetic_mcp_tool_boundary","network_access":false,"side_effects":false,"scenarios":[)";
  bool first=true;
  for(const auto& c:scenarios()) {
    if(!first) s+=',';
    first=false;
    s+="{\"id\":\""+std::string(c.id)+"\",\"domain\":\""+std::string(c.domain)+"\"}";
  }
  return s+"]}";
}
std::string assess(std::string_view id,std::string_view role) {
  if(role!="reviewer")return deny();
  for(const auto& c:scenarios())if(c.id==id)
    return "{\"ok\":true,\"mode\":\"simulation_only\",\"scenario\":\""+std::string(c.id)+"\",\"control\":\""+std::string(c.control)+"\",\"network_access\":false,\"side_effects\":false}";
  return deny();
}
std::string simulate(std::string_view id,std::string_view role) {
  if(role!="reviewer")return deny();
  for(const auto& c:scenarios())if(c.id==id) {
    const auto result=check(failing_fixture(c.failing_gate));
    if(result.allowed || result.failed_gate!=c.failing_gate)return deny();
    return "{\"ok\":true,\"mode\":\"simulation_only\",\"decision\":\"no_go\",\"scenario\":\""+std::string(c.id)+"\",\"blocked_gate\":\""+std::string(result.failed_gate)+"\",\"network_access\":false,\"side_effects\":false}";
  }
  return deny();
}
std::string baseline(std::string_view role) {
  if(role!="reviewer" || !check(Evidence{}).allowed)return deny();
  return R"({"ok":true,"mode":"simulation_only","decision":"allow_preview","transport":"offline_fixture","tool":"monitoring.health","resource":"local_fixture","network_access":false,"side_effects":false,"real_mcp_calls":0})";
}
std::string evaluate(std::string_view role, std::string_view tenant, std::string_view source,
                     std::string_view registry, std::string_view mutation, std::string_view approval) {
  Evidence e;
  e.role=role; e.tenant=tenant; e.source=source; e.registry=registry; e.mutation=mutation; e.approval=approval;
  if(!check(e).allowed)return deny();
  return baseline("reviewer");
}
std::string report() {
  return R"({"ok":true,"mode":"simulation_only","scenario_count":32,"policy_gate_count":33,"domain_count":19,"real_mcp_calls":0,"real_servers_connected":0,"real_ui_rendering":0,"network_access":false,"side_effects":false})";
}
} // namespace simulator::mcp_tool_security
