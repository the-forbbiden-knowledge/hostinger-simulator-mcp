#include "simulator/vector_security.hpp"
#include <iostream>
#include <set>
#include <string_view>
namespace {
int checked=0,errors=0;
void check(bool condition, std::string_view name){++checked;if(!condition){++errors;std::cerr<<"FAIL: "<<name<<'\n';}}
bool has(std::string_view text,std::string_view word){return text.find(word)!=std::string_view::npos;}
}
int main(){
  using namespace simulator::vector_security;
  check(scenarios().size()==22,"22 cases");
  std::set<std::string_view> ids;
  for(const auto& sc:scenarios()){
    check(ids.insert(sc.id).second,"unique id");
    check(!sc.threat.empty()&&!sc.required_control.empty()&&!sc.failed_gate.empty(),"complete scenario");
    const auto result=simulate(sc.id,"reviewer");
    check(has(result,"\"decision\":\"deny\"")&&has(result,sc.failed_gate),"negative gate denies");
    check(has(result,"\"network_access\":false")&&has(result,"\"side_effects\":false"),"no side effects");
    check(has(assess(sc.id,"reviewer"),"\"decision\":\"allow_preview\""),"reviewer describes fixture");
    check(assess(sc.id,"viewer")==assess("unknown","reviewer"),"uniform unknown and unauthorized");
    check(simulate(sc.id,"viewer")==simulate("unknown","reviewer"),"uniform denial simulation");
  }
  RetrievalEvidence e;
  check(verify_fixture(e).accepted,"baseline passes");
  e.tenant_scope="other_tenant";check(verify_fixture(e).failed_gate=="tenant_scope","tenant fail-closed");e=RetrievalEvidence{};
  e.session_scope="other_session";check(verify_fixture(e).failed_gate=="session_scope","session fail-closed");e=RetrievalEvidence{};
  e.classification="secret";check(verify_fixture(e).failed_gate=="classification","classification fail-closed");e=RetrievalEvidence{};
  e.classification="internal";e.role="operator";check(verify_fixture(e).failed_gate=="role","operator internal denied");e=RetrievalEvidence{};
  e.classification="internal";e.role="reviewer";check(verify_fixture(e).accepted,"reviewer internal synthetic policy");e=RetrievalEvidence{};
  e.purpose="export";check(verify_fixture(e).failed_gate=="purpose","export purpose denied");e=RetrievalEvidence{};
  e.result_kind="raw_vector";check(verify_fixture(e).failed_gate=="result_kind","raw vector result denied");e=RetrievalEvidence{};
  e.result_kind="raw_chunk";check(verify_fixture(e).failed_gate=="result_kind","raw chunk result denied");e=RetrievalEvidence{};
  e.top_k=0;check(verify_fixture(e).failed_gate=="top_k","top k lower");e=RetrievalEvidence{};
  e.top_k=6;check(verify_fixture(e).failed_gate=="top_k","top k upper");e=RetrievalEvidence{};
  e.daily_query_count=101;check(verify_fixture(e).failed_gate=="query_budget","budget");e=RetrievalEvidence{};
  e.model_metadata_private=false;check(verify_fixture(e).failed_gate=="metadata","metadata");e=RetrievalEvidence{};
  e.input_data_minimized=false;check(verify_fixture(e).failed_gate=="data_minimization","minimization");e=RetrievalEvidence{};
  e.vector_export_disabled=false;check(verify_fixture(e).failed_gate=="vector_export","vector export blocked");e=RetrievalEvidence{};
  e.raw_chunk_export_disabled=false;check(verify_fixture(e).failed_gate=="raw_chunk_export","chunk export blocked");e=RetrievalEvidence{};
  check(query("operator","same_tenant","public","search","redacted_summary").find("synthetic_redacted_summary")!=std::string::npos,"operator safe preview");
  check(has(query("operator","same_tenant","internal","search","redacted_summary"),"\"decision\":\"deny\""),"operator sensitive denied");
  check(has(query("reviewer","other_tenant","public","search","redacted_summary"),"\"decision\":\"deny\""),"tenant denied via CLI");
  check(has(query("reviewer","same_tenant","public","export","raw_vector"),"\"decision\":\"deny\""),"raw export via CLI denied");
  check(baseline("viewer")==assess("unknown","reviewer"),"baseline unauthorized uniformly denied");
  check(has(baseline("reviewer"),"\"raw_vectors_returned\":0"),"no raw vector in positive");
  check(has(report(),"\"real_inference_risk_measured\":null"),"no measured risk invented");
  check(has(list_scenarios(),"\"scenario_kind\":\"synthetic_vector_privacy\""),"synthetic inventory marker");
  if(errors){std::cerr<<errors<<"/"<<checked<<" failed\n";return 1;}
  std::cout<<"Vector privacy fixtures: "<<checked<<" assertions passed\n";
}
