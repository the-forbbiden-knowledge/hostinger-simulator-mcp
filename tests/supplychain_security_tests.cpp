#include "simulator/supplychain_security.hpp"
#include <iostream>
#include <set>
#include <string_view>

namespace {
int checked = 0;
int errors = 0;
void check(bool good, std::string_view message) {
  ++checked;
  if (!good) { ++errors; std::cerr << "FAIL: " << message << '\n'; }
}
bool has(std::string_view haystack, std::string_view needle) {return haystack.find(needle)!=std::string_view::npos;}
}

int main() {
  using namespace simulator::supplychain_security;
  check(scenarios().size()==26,"26 negative cases");
  std::set<std::string_view> unique;
  for (const auto& sc:scenarios()) {
    check(unique.insert(sc.id).second,"unique scenario id");
    check(!sc.layer.empty()&&!sc.threat.empty()&&!sc.required_control.empty(),"complete control mapping");
    const auto r=simulate(sc.id,"reviewer");
    check(has(r,"\"decision\":\"deny\"")&&has(r,sc.failed_gate),"matching failed gate");
    check(has(r,"\"side_effects\":false")&&has(r,"\"network_access\":false"),"offline negative path");
    check(has(assess(sc.id,"reviewer"),"\"decision\":\"allow_preview\""),"reviewer can see risk");
    check(assess(sc.id,"viewer")==assess("unknown","reviewer"),"uniform unauthorized/unknown response");
    check(simulate(sc.id,"operator")==simulate("unknown","reviewer"),"simulation restricted to review fixtures");
  }
  check(has(baseline("reviewer"),"\"decision\":\"allow_preview\""),"safe positive fixture");
  check(has(baseline("reviewer"),"\"cryptographic_hash_computed\":false"),"no crypto verification claimed");
  check(baseline("viewer")==assess("unknown","reviewer"),"positive fixture denied to viewer");
  check(has(report(),"\"actual_detection_rate\":null"),"no real detection rate claimed");
  check(has(report(),"\"real_artifacts_verified\":0"),"no genuine artifacts evaluated");
  check(has(list_scenarios(),"\"scenario_kind\":\"synthetic_ai_supply_chain\""),"inventory explicit synthetic");
  ArtifactEvidence e;
  check(verify_fixture(e).accepted,"all baseline fixture gates pass");
  e.source_registry="lookalike.example.invalid"; check(!verify_fixture(e).accepted&&verify_fixture(e).failed_gate=="approved_registry","reject registry confusion"); e=ArtifactEvidence{};
  e.namespace_name="simulator/approv3d"; check(verify_fixture(e).failed_gate=="namespace_pin","reject namespace typo"); e=ArtifactEvidence{};
  e.pinned_version="latest"; check(verify_fixture(e).failed_gate=="version_pin","reject mutable version"); e=ArtifactEvidence{};
  e.observed_sha256="bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"; check(verify_fixture(e).failed_gate=="sha256_match","reject changed hash"); e=ArtifactEvidence{};
  e.observed_sha256="AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"; check(verify_fixture(e).failed_gate=="sha256_format","reject unsupported casing"); e=ArtifactEvidence{};
  e.expected_sha256="not_sha"; check(verify_fixture(e).failed_gate=="sha256_format","reject invalid expected digest"); e=ArtifactEvidence{};
  e.artifact_format="pickle"; check(verify_fixture(e).failed_gate=="safe_format","reject unsafe serializer"); e=ArtifactEvidence{};
  e.artifact_format="joblib"; check(verify_fixture(e).failed_gate=="safe_format","reject joblib"); e=ArtifactEvidence{};
  e.signature_verified=false; check(verify_fixture(e).failed_gate=="signature","signature evidence required"); e=ArtifactEvidence{};
  e.provenance_verified=false; check(verify_fixture(e).failed_gate=="provenance","provenance evidence required"); e=ArtifactEvidence{};
  e.license_approved=false; check(verify_fixture(e).failed_gate=="license","approved license required"); e=ArtifactEvidence{};
  e.inventory_recorded=false; check(verify_fixture(e).failed_gate=="inventory","inventory required"); e=ArtifactEvidence{};
  e.isolated_review=false; check(verify_fixture(e).failed_gate=="isolation","isolation required"); e=ArtifactEvidence{};
  e.behavior_reviewed=false; check(verify_fixture(e).failed_gate=="behavior","behavior review required"); e=ArtifactEvidence{};
  e.human_approved=false; check(verify_fixture(e).failed_gate=="approval","approval required"); e=ArtifactEvidence{};
  e.component_kind="unknown"; check(verify_fixture(e).failed_gate=="component_kind","unknown category denied"); e=ArtifactEvidence{};
  e.component_kind="dataset"; e.artifact_format="jsonl"; check(verify_fixture(e).accepted,"valid synthetic dataset fixture");e=ArtifactEvidence{};
  e.component_kind="tokenizer"; e.artifact_format="json"; check(verify_fixture(e).accepted,"valid synthetic tokenizer fixture");e=ArtifactEvidence{};
  e.component_kind="mcp_server";e.artifact_format="manifest_json";check(verify_fixture(e).accepted,"valid synthetic MCP server fixture");e=ArtifactEvidence{};
  e.component_kind="hardware";e.artifact_format="manifest_json";check(verify_fixture(e).accepted,"valid synthetic hardware fixture");
  if(errors){std::cerr << errors << "/" << checked << " failed\n";return 1;}
  std::cout << "Synthetic supply-chain policy: " << checked << " assertions passed\n";
  return 0;
}
