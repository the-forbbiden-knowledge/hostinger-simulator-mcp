#include "simulator/infrastructure_security.hpp"
#include <string>

namespace simulator::infrastructure_security {
namespace {
std::string deny(){return R"({"ok":false,"decision":"deny","error":"not_available_or_authorized","mode":"simulation_only","network_access":false,"side_effects":false})";}
PolicyEvidence violate(std::string_view gate) {
  PolicyEvidence e;
  if(gate=="role") e.role="untrusted";
  if(gate=="scope") e.scope="out_of_scope";
  if(gate=="tenant_scope") e.tenant_scope="other_tenant";
  if(gate=="cloud_identity") e.cloud_identity="admin";
  if(gate=="namespace_scope") e.namespace_scope="other_namespace";
  if(gate=="endpoint_auth") e.endpoint_auth="public_anonymous";
  if(gate=="storage_policy") e.storage_policy="public";
  if(gate=="credentials") e.credentials="long_lived";
  if(gate=="secrets") e.secrets="in_plaintext";
  if(gate=="service_account") e.service_account="default_auto_mount";
  if(gate=="rbac") e.rbac="wildcard_write";
  if(gate=="admission") e.admission="disabled";
  if(gate=="pod_privilege") e.pod_privilege="privileged";
  if(gate=="host_mount") e.host_mount="host_path";
  if(gate=="runtime") e.runtime="host_shared";
  if(gate=="gpu_isolation") e.gpu_isolation="unrestricted";
  if(gate=="image_provenance") e.image_provenance="unverified";
  if(gate=="build_attestation") e.build_attestation="missing";
  if(gate=="egress") e.egress="public";
  if(gate=="audit") e.audit="unredacted";
  if(gate=="approval") e.approval="missing";
  if(gate=="activity") e.activity="production_write";
  if(gate=="artifact_origin") e.artifact_origin="unknown";
  if(gate=="policy_source") e.policy_source="model_output";
  if(gate=="external_access") e.no_external_access=false;
  if(gate=="resource_limits") e.resource_limits=false;
  return e;
}
} // namespace

Decision check_policy(const PolicyEvidence& e) noexcept {
  if(e.role!="reviewer") return {false,"role"};
  if(e.scope!="approved_fixture") return {false,"scope"};
  if(e.tenant_scope!="same_tenant") return {false,"tenant_scope"};
  if(e.cloud_identity!="least_privilege") return {false,"cloud_identity"};
  if(e.namespace_scope!="same_namespace") return {false,"namespace_scope"};
  if(e.endpoint_auth!="private_authenticated") return {false,"endpoint_auth"};
  if(e.storage_policy!="private_scoped") return {false,"storage_policy"};
  if(e.credentials!="short_lived") return {false,"credentials"};
  if(e.secrets!="redacted") return {false,"secrets"};
  if(e.service_account!="dedicated_no_automount") return {false,"service_account"};
  if(e.rbac!="minimal_read_only") return {false,"rbac"};
  if(e.admission!="restricted") return {false,"admission"};
  if(e.pod_privilege!="unprivileged") return {false,"pod_privilege"};
  if(e.host_mount!="none") return {false,"host_mount"};
  if(e.runtime!="sandboxed") return {false,"runtime"};
  if(e.gpu_isolation!="isolated") return {false,"gpu_isolation"};
  if(e.image_provenance!="approved_digest") return {false,"image_provenance"};
  if(e.build_attestation!="verified_fixture") return {false,"build_attestation"};
  if(e.egress!="denied") return {false,"egress"};
  if(e.audit!="redacted_audited") return {false,"audit"};
  if(e.approval!="approved") return {false,"approval"};
  if(e.activity!="read_only_preview") return {false,"activity"};
  if(e.artifact_origin!="synthetic") return {false,"artifact_origin"};
  if(e.policy_source!="trusted") return {false,"policy_source"};
  if(!e.no_external_access) return {false,"external_access"};
  if(!e.resource_limits) return {false,"resource_limits"};
  return {true,"none"};
}

const std::vector<Scenario>& scenarios(){
  static const std::vector<Scenario> cases{
    {"broad_ml_execution_role","cloud_iam","Overbroad inference role crosses workload scope","Workload least privilege and explicit resource scoping","cloud_identity"},
    {"unbounded_role_trust","cloud_iam","Role trust spans unapproved environment","Bind the caller and trust policy to verified scope","scope"},
    {"cross_tenant_assume_role","cloud_iam","Identity may traverse a tenant boundary","Independent tenant binding before every tool action","tenant_scope"},
    {"model_endpoint_without_auth","inference","Inference endpoint lacks independently verified identity","Private authenticated inference endpoint","endpoint_auth"},
    {"model_bucket_public_read","storage","Model artifact store is publicly readable","Private storage scoped to required principal","storage_policy"},
    {"training_dataset_cross_tenant","storage","Training dataset read across tenant boundary","Tenant isolation enforced before storage read","tenant_scope"},
    {"long_lived_cloud_credentials","credentials","Non-expiring credential used by workload","Short-lived workload-bound credentials","credentials"},
    {"secrets_in_model_metadata","credentials","Secrets included in model registry metadata","Redact sensitive metadata before publishing","secrets"},
    {"logs_with_identity_tokens","telemetry","Sensitive tokens appear in diagnostics","Redacted structured logs and trace controls","audit"},
    {"unapproved_external_callback","network","Workload contacts unapproved destination","No outbound network access in offline fixture","egress"},
    {"service_account_auto_token","kubernetes","Pod automatically receives API token without need","Dedicated ServiceAccount and token automount disabled","service_account"},
    {"namespace_cross_access","kubernetes","Workload accesses another namespace","Bind namespace to trusted workload identity","namespace_scope"},
    {"rolebinding_wildcard","kubernetes","RBAC grants wildcard rights on resources","Restrict verbs and resources to minimal read-only scope","rbac"},
    {"pod_admission_not_enforced","kubernetes","Pod admission admits unsafe specification","Enforce restricted admission before scheduling","admission"},
    {"privileged_container","container","Workload requests privileged container execution","Deny privileged execution","pod_privilege"},
    {"host_filesystem_mount","container","Workload requests access to host filesystem","Disallow host path mounts","host_mount"},
    {"host_namespace_sharing","container","Workload can reach host process or network namespace","Enforce runtime isolation independent of app permissions","runtime"},
    {"gpu_device_broad_mapping","accelerator","Accelerator device access exceeds authorized isolation","Explicit GPU device allocation and isolation review","gpu_isolation"},
    {"unsigned_inference_image","supply_chain","Serving image lacks approved digest","Pinned and approved container image digest","image_provenance"},
    {"unverified_training_build","supply_chain","Build output lacks required attestation evidence","Require verified synthetic build evidence","build_attestation"},
    {"policy_data_from_model_output","policy","Model response attempts to become deployment authority","Only trusted service context can set policy source","policy_source"},
    {"unapproved_deployment_change","governance","Deployment preview lacks an accountable approval","Independent authorization for deployment activities","approval"},
    {"production_mutation_attempt","governance","Unsafe production change requested","Read-only synthetic previews only","activity"},
    {"unapproved_artifact_origin","supply_chain","Model or runtime artifact originates outside lab","Allow only synthetic authorized fixture metadata","artifact_origin"},
    {"live_provider_api_access","network","Scenario would call a real provider API","No external access under local simulation boundary","external_access"},
    {"resource_limits_missing","container","Workload has no bounded resources","Enforce resource requests and limits in deployment policy","resource_limits"},
    {"untrusted_operator_role","identity","Unknown user role claims to approve deployments","Reviewer role must be trusted service context","role"},
    {"model_log_export","telemetry","Diagnostic data disclosed beyond allowed recipients","Redacted auditable export policy","audit"},
  };
  return cases;
}
std::string list_scenarios(){
  std::string result=R"({"ok":true,"mode":"simulation_only","scenario_kind":"synthetic_infrastructure_hardening","network_access":false,"side_effects":false,"scenarios":[)";
  bool first=true;
  for(const auto& s:scenarios()){
    if(!first) result+=',';
    first=false;
    result+="{\"id\":\""+std::string(s.id)+"\",\"category\":\""+std::string(s.category)+"\"}";
  }
  return result+"]}";
}
std::string assess(std::string_view id,std::string_view role){
  if(role!="reviewer") return deny();
  for(const auto& s:scenarios()) if(id==s.id)
    return "{\"ok\":true,\"mode\":\"simulation_only\",\"scenario\":\""+std::string(s.id)+"\",\"control\":\""+std::string(s.control)+"\",\"network_access\":false,\"side_effects\":false}";
  return deny();
}
std::string simulate(std::string_view id,std::string_view role){
  if(role!="reviewer") return deny();
  for(const auto& s:scenarios()) if(id==s.id){
    const auto result=check_policy(violate(s.failing_gate));
    if(result.allowed_preview || result.failed_gate!=s.failing_gate) return deny();
    return "{\"ok\":true,\"mode\":\"simulation_only\",\"decision\":\"no_go\",\"scenario\":\""+std::string(s.id)+"\",\"blocked_gate\":\""+std::string(result.failed_gate)+"\",\"network_access\":false,\"side_effects\":false}";
  }
  return deny();
}
std::string baseline(std::string_view role){
  if(role!="reviewer" || !check_policy(PolicyEvidence{}).allowed_preview) return deny();
  return R"({"ok":true,"mode":"simulation_only","decision":"allow_preview","resource":"local_fixture","action":"read_only_metadata","real_cloud_checks":0,"real_clusters_checked":0,"network_access":false,"side_effects":false})";
}
std::string evaluate(std::string_view role,std::string_view scope,std::string_view cloud_identity,
                     std::string_view namespace_scope,std::string_view activity,std::string_view approval){
  PolicyEvidence e;
  e.role=role;e.scope=scope;e.cloud_identity=cloud_identity;
  e.namespace_scope=namespace_scope;e.activity=activity;e.approval=approval;
  if(!check_policy(e).allowed_preview) return deny();
  return baseline("reviewer");
}
std::string report(){
  return R"({"ok":true,"mode":"simulation_only","scenario_count":28,"domains":["cloud_iam","inference","storage","credentials","telemetry","network","kubernetes","container","accelerator","supply_chain","policy","governance","identity"],"synthetic_policy_gate_count":26,"real_cloud_checks":0,"real_clusters_checked":0,"gpu_devices_tested":0,"network_access":false,"side_effects":false})";
}
} // namespace simulator::infrastructure_security
