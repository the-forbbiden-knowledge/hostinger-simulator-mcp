#include "simulator/capstone_security.hpp"
#include <iostream>
#include <set>
#include <string_view>

namespace {
int failures=0;
int checks=0;
void check(bool condition, std::string_view message) {
  ++checks;
  if(!condition){++failures; std::cerr << "FAIL " << message << '\n';}
}
bool has(std::string_view s, std::string_view what) {
  return s.find(what) != std::string_view::npos;
}
}
int main(){
  using namespace simulator::capstone_security;
  const auto& all=scenarios();
  check(all.size()==17,"17 offline cross-zone cases");
  std::set<std::string_view> unique;
  const auto denial=simulate("missing", "untrusted", "viewer", "cross_session", "none");
  for(const auto& sc:all){
    check(unique.insert(sc.id).second,"unique case identifier");
    check(!sc.boundary.empty()&&!sc.risk.empty()&&!sc.required_control.empty(),"mapped control");
    const auto read=assess(sc.id,"reviewer");
    check(has(read,"\"decision\":\"allow_preview\""),"reviewer preview");
    check(has(read,"\"side_effects\":false")&&has(read,"\"network_access\":false"),"offline preview");
    check(assess(sc.id,"viewer")==denial,"reviewer only");
    check(assess(sc.id,"operator")==denial,"operator not privileged reviewer");
    check(simulate(sc.id,"trusted_application","reviewer","same_session","approved")==denial,"scenario never authorizes unsafe transition");
    check(simulate(sc.id,"retrieved_document","reviewer","same_session","approved")==denial,"retrieval has no authority");
  }
  check(assess("missing","reviewer")==denial,"unknown not distinguishable");
  check(assess("public_ai_database_broker","admin")==denial,"unknown role denied");
  check(simulate("safe_health_preview","trusted_application","operator","same_session","approved")!=denial,"positive fixture");
  check(simulate("safe_health_preview","trusted_application","operator","cross_session","approved")==denial,"cross session denied");
  check(simulate("safe_health_preview","web_page","operator","same_session","approved")==denial,"untrusted source denied");
  check(simulate("safe_health_preview","trusted_application","operator","same_session","none")==denial,"missing approval denied");
  check(simulate("safe_health_preview","trusted_application","viewer","same_session","approved")==denial,"viewer denied");
  check(has(report(),"\"real_attacks_executed\":0")&&has(report(),"\"real_detection_rate\":null"),"report doesn't make real-world claims");
  check(has(list_scenarios(),"\"network_access\":false"),"inventory offline");
  check(failures==0,"all checks passed");
  if(failures){std::cerr << failures << "/" << checks << " failed\n";return 1;}
  std::cout << "Cross-zone control model: " << checks << " assertions passed\n";
  return 0;
}
