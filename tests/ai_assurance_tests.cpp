#include "simulator/ai_assurance.hpp"
#include <iostream>
#include <cstdlib>
#include <set>
using namespace simulator::ai_assurance;
int main(){int n=0;auto require=[&](bool b,const char* msg){++n;if(!b){std::cerr<<"FAIL: "<<msg<<'\n';std::exit(1);}};
require(scenarios().size()==24,"scenario count");require(check(baseline_fixture()).allowed,"baseline");
std::set<std::string_view> ids;
for(const auto& c:scenarios()){
require(ids.insert(c.id).second,"unique ID");require(!c.risk.empty()&&!c.control.empty(),"described");
auto d=check(bad_fixture(c.failing_gate));require(!d.allowed,"fail closed");require(d.failed_gate==c.failing_gate,"gate matched");
require(simulate(c.id,"reviewer").find("\"decision\":\"no_go\"")!=std::string::npos,"negative fixture");
require(simulate(c.id,"reviewer").find("\"network_access\":false")!=std::string::npos,"offline");
require(assess(c.id,"viewer")==assess("unknown","viewer"),"opaque viewer");
}
require(!check(bad_fixture("no_live_access")).allowed,"deny live access");
auto incomplete=baseline_fixture();incomplete.gates.pop_back();require(!check(incomplete).allowed,"missing gate");
require(baseline("reviewer").find("allow_preview")!=std::string::npos,"approved");
require(baseline("viewer")==assess("unknown","viewer"),"uniform response");
require(decide("reviewer","approved_fixture","corroborated","approved_read_only","redacted_trace").find("allow_preview")!=std::string::npos,"safe decision");
require(decide("reviewer","approved_fixture","unverified","approved_read_only","redacted_trace").find("deny")!=std::string::npos,"evidence first");
require(decide("reviewer","approved_fixture","corroborated","unapproved_action","redacted_trace").find("deny")!=std::string::npos,"approval required");
require(decide("reviewer","out_of_scope","corroborated","approved_read_only","redacted_trace").find("deny")!=std::string::npos,"scope");
require(impact("reviewer","approved_fixture","bounded","documented","unknown_not_invented").find("\"estimated_loss\":null")!=std::string::npos,"unknown is unknown");
require(impact("reviewer","approved_fixture","bounded","documented","invented").find("deny")!=std::string::npos,"financial error");
require(report().find("\"scenario_count\":24")!=std::string::npos,"report");
require(list().find("unapproved_mission")!=std::string::npos,"inventory");
std::cout<<"AI assurance policy: "<<n<<" independent checks passed\n";
}
