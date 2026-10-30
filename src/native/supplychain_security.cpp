#include "simulator/supplychain_security.hpp"
#include "simulator/simulator.hpp"
#include <algorithm>
#include <cstddef>

namespace simulator::supplychain_security {
namespace {
std::string deny() {
  // Same response for unknown cases and unauthorized actors; no asset probing.
  return R"({"ok":true,"mode":"simulation_only","decision":"deny","reason":"not_authorized_or_unavailable","network_access":false,"side_effects":false})";
}

bool sha256_hex(std::string_view value) noexcept {
  if (value.size() != 64) return false;
  return std::all_of(value.begin(), value.end(), [](char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
  });
}

bool kind_allowed(std::string_view kind) noexcept {
  constexpr std::string_view values[] = {
    "model", "adapter", "dataset", "tokenizer", "dependency",
    "mcp_server", "infrastructure", "hardware", "third_party_service"
  };
  return std::find(std::begin(values), std::end(values), kind) != std::end(values);
}

bool format_allowed(std::string_view kind, std::string_view format) noexcept {
  if (kind == "model" || kind == "adapter") return format == "safetensors";
  if (kind == "dataset") return format == "jsonl";
  if (kind == "tokenizer") return format == "json";
  return format == "manifest_json";
}

ArtifactEvidence fixture_for(const Scenario& sc) {
  ArtifactEvidence e;
  if (sc.layer == "data") {
    e.component_kind = "dataset";
    e.artifact_format = "jsonl";
  } else if (sc.layer == "model" || sc.layer == "serialization") {
    e.component_kind = "model";
    e.artifact_format = "safetensors";
  } else if (sc.layer == "adapter") {
    e.component_kind = "adapter";
    e.artifact_format = "safetensors";
  } else if (sc.layer == "tokenizer") {
    e.component_kind = "tokenizer";
    e.artifact_format = "json";
  } else if (sc.layer == "mcp") {
    e.component_kind = "mcp_server";
    e.artifact_format = "manifest_json";
  } else if (sc.layer == "infrastructure") {
    e.component_kind = "infrastructure";
    e.artifact_format = "manifest_json";
  } else if (sc.layer == "hardware") {
    e.component_kind = "hardware";
    e.artifact_format = "manifest_json";
  } else if (sc.layer == "third_party_service") {
    e.component_kind = "third_party_service";
    e.artifact_format = "manifest_json";
  } else {
    e.component_kind = "dependency";
    e.artifact_format = "manifest_json";
  }

  const auto gate = sc.failed_gate;
  if (gate == "approved_registry") e.source_registry = "unapproved.example.invalid";
  else if (gate == "namespace_pin") e.namespace_name = "unknown/mutable";
  else if (gate == "version_pin") e.pinned_version = "latest";
  else if (gate == "sha256_match") e.observed_sha256 = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
  else if (gate == "sha256_format") e.observed_sha256 = "unverified";
  else if (gate == "safe_format") e.artifact_format = "pickle";
  else if (gate == "signature") e.signature_verified = false;
  else if (gate == "provenance") e.provenance_verified = false;
  else if (gate == "license") e.license_approved = false;
  else if (gate == "inventory") e.inventory_recorded = false;
  else if (gate == "isolation") e.isolated_review = false;
  else if (gate == "behavior") e.behavior_reviewed = false;
  else if (gate == "approval") e.human_approved = false;
  return e;
}
}

Decision verify_fixture(const ArtifactEvidence& e) noexcept {
  if (!kind_allowed(e.component_kind)) return {false, "component_kind"};
  if (e.source_registry != "registry.example.invalid") return {false, "approved_registry"};
  if (e.namespace_name != "simulator/approved") return {false, "namespace_pin"};
  if (e.pinned_version != "1.0.0") return {false, "version_pin"};
  if (!sha256_hex(e.expected_sha256) || !sha256_hex(e.observed_sha256)) return {false, "sha256_format"};
  if (e.expected_sha256 != e.observed_sha256) return {false, "sha256_match"};
  if (!format_allowed(e.component_kind, e.artifact_format)) return {false, "safe_format"};
  if (!e.signature_verified) return {false, "signature"};
  if (!e.provenance_verified) return {false, "provenance"};
  if (!e.license_approved) return {false, "license"};
  if (!e.inventory_recorded) return {false, "inventory"};
  if (!e.isolated_review) return {false, "isolation"};
  if (!e.behavior_reviewed) return {false, "behavior"};
  if (!e.human_approved) return {false, "approval"};
  return {true, "none"};
}

const std::vector<Scenario>& scenarios() {
  static const std::vector<Scenario> items{
    {"mcp_repository_tampering", "mcp", "unreviewed_mcp_server_change", "signed_source_release_and_independent_review", "signature"},
    {"mcp_tool_metadata_drift", "mcp", "tool_schema_changed_after_approval", "immutable_tool_catalog_and_signed_release", "provenance"},
    {"mcp_third_party_server", "mcp", "unknown_mcp_server_publisher", "approved_registry_and_publisher_identity", "approved_registry"},
    {"dependency_confusion", "software", "unexpected_package_registry_precedence", "approved_registry_and_private_namespace", "approved_registry"},
    {"package_namespace_typo", "software", "lookalike_package_namespace", "exact_namespace_pin", "namespace_pin"},
    {"mutable_dependency", "software", "unversioned_package_update", "exact_component_version_lock", "version_pin"},
    {"dependency_digest_change", "software", "changed_package_bytes", "recomputed_digest_vs_known_good_manifest", "sha256_match"},
    {"artifact_digest_malformed", "software", "invalid_integrity_attestation", "strict_sha256_encoding_and_independent_hashing", "sha256_format"},
    {"dependency_license_missing", "software", "license_not_reviewed", "license_policy_gate", "license"},
    {"dependency_inventory_gap", "software", "component_not_in_release_inventory", "sbom_aibom_manifest", "inventory"},
    {"serialized_pickle_model", "serialization", "executable_deserializer_format", "data_only_safe_serialization", "safe_format"},
    {"serialized_joblib_model", "serialization", "unsafe_serialization_family", "data_only_safe_serialization", "safe_format"},
    {"model_registry_reuse", "model", "model_name_reassigned_to_new_supplier", "immutable_registry_namespace_and_digest", "namespace_pin"},
    {"model_weight_tampering", "model", "unexpected_model_weight_changes", "trusted_digest_signature_and_behavior_review", "sha256_match"},
    {"model_behavior_drift", "model", "reviewed_model_produces_changed_behavior", "behavioral_regression_gate", "behavior"},
    {"training_dataset_poisoning", "data", "unreviewed_training_records", "dataset_lineage_and_integrity_review", "provenance"},
    {"training_dataset_revision", "data", "unapproved_dataset_change", "signed_dataset_release", "signature"},
    {"lora_adapter_tamper", "adapter", "unexpected_adapter_weight_delta", "adapter_integrity_and_behavioral_review", "behavior"},
    {"tokenizer_vocabulary_change", "tokenizer", "unreviewed_tokenizer_mapping", "tokenizer_manifest_and_revision_approval", "approval"},
    {"unsafe_external_model_data", "serialization", "untrusted_external_model_references", "isolate_and_review_imported_artifacts", "isolation"},
    {"build_runner_integrity", "infrastructure", "untrusted_build_worker_images", "isolated_pinned_build_environment", "isolation"},
    {"unreviewed_pipeline_action", "infrastructure", "mutable_ci_workflow_dependency", "pinned_and_reviewed_ci_actions", "version_pin"},
    {"hardware_firmware_provenance", "hardware", "unknown_firmware_source", "verified_supplier_and_firmware_provenance", "provenance"},
    {"third_party_provider_drift", "third_party_service", "unapproved_service_change", "vendor_inventory_and_approval", "approval"},
    {"scanner_only_approval", "model", "static_scan_only_misses_behavioral_risk", "defense_in_depth_and_behavioral_review", "behavior"},
    {"missing_human_promotion", "model", "unapproved_artifact_promotion", "reviewer_approved_release_promotion", "approval"}
  };
  return items;
}

std::string list_scenarios() {
  std::string out = R"({"ok":true,"mode":"simulation_only","scenario_kind":"synthetic_ai_supply_chain","network_access":false,"side_effects":false,"scenarios":[)";
  bool first = true;
  for (const auto& sc : scenarios()) {
    if (!first) out += ',';
    first = false;
    out += R"({"id":")" + simulator::escape_json(sc.id) +
           R"(","layer":")" + simulator::escape_json(sc.layer) + R"(","status":"fixture_only"})";
  }
  return out + R"(]})";
}

std::string assess(std::string_view id, std::string_view role) {
  if (role != "reviewer") return deny();
  const auto& all = scenarios();
  const auto it = std::find_if(all.begin(), all.end(), [id](const Scenario& x){ return x.id == id; });
  if (it == all.end()) return deny();
  return R"({"ok":true,"mode":"simulation_only","decision":"allow_preview","scenario":")" +
     simulator::escape_json(it->id) + R"(","layer":")" + simulator::escape_json(it->layer) +
     R"(","threat":")" + simulator::escape_json(it->threat) +
     R"(","required_control":")" + simulator::escape_json(it->required_control) +
     R"(","failed_fixture_gate":")" + simulator::escape_json(it->failed_gate) +
     R"(","evidence":"synthetic_fixture_only","network_access":false,"side_effects":false})";
}

std::string simulate(std::string_view id, std::string_view role) {
  if (role != "reviewer") return deny();
  const auto& all = scenarios();
  const auto it = std::find_if(all.begin(), all.end(), [id](const Scenario& x){ return x.id == id; });
  if (it == all.end()) return deny();
  const auto result = verify_fixture(fixture_for(*it));
  // All defined negative scenarios are designed to fail at exactly one gate.
  if (result.accepted || result.failed_gate != it->failed_gate) return deny();
  return R"({"ok":true,"mode":"simulation_only","decision":"deny","scenario":")" +
    simulator::escape_json(it->id) + R"(","failed_fixture_gate":")" +
    simulator::escape_json(result.failed_gate) +
    R"(","observed_effect":"synthetic_rejection_only","network_access":false,"side_effects":false})";
}

std::string baseline(std::string_view role) {
  if (role != "reviewer") return deny();
  const ArtifactEvidence trusted;
  const auto d = verify_fixture(trusted);
  if (!d.accepted) return deny();
  return R"({"ok":true,"mode":"simulation_only","decision":"allow_preview","scenario":"approved_fixture","evidence":"synthetic_metadata_only","cryptographic_hash_computed":false,"signature_cryptographically_verified":false,"artifact_loaded":false,"network_access":false,"side_effects":false})";
}

std::string report() {
  return R"({"ok":true,"mode":"simulation_only","report":"ai_supply_chain_control_model","modeled_negative_cases":)" +
    std::to_string(scenarios().size()) +
    R"(,"real_artifacts_verified":0,"cryptographic_verification_operational":false,"actual_detection_rate":null,"network_access":false,"side_effects":false})";
}
} // namespace simulator::supplychain_security
