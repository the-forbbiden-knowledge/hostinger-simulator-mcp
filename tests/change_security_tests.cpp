#include "simulator/change_security.hpp"
#include <cstdlib>
#include <iostream>
#include <set>
#include <string>
#include <string_view>
using namespace simulator::change_security;
int main() {
  int n = 0;
  auto require = [&n](bool success, const char* msg) {
    ++n;
    if (!success) { std::cerr << "FAIL: " << msg << '\n'; std::exit(1); }
  };
  require(scenarios().size() == 12, "scenario inventory");
  require(check(baseline_fixture()).allowed, "safe baseline preview");
  std::set<std::string_view> ids;
  for (const auto& scenario : scenarios()) {
    require(ids.insert(scenario.id).second, "unique scenario id");
    require(!scenario.domain.empty(), "described domain");
    auto decision = check(bad_fixture(scenario.failed_gate));
    require(!decision.allowed, "fail closed");
    require(decision.failed_gate == scenario.failed_gate, "correct gate attribution");
    auto output = simulate(scenario.id, "reviewer");
    require(output.find("\"decision\":\"no_go\"") != std::string::npos, "negative fixture");
    require(output.find("\"network_access\":false") != std::string::npos, "offline fixture");
    require(assess(scenario.id, "viewer") == assess("unknown", "viewer"), "opaque unauthorized assessment");
  }
  require(!check(bad_fixture("offline")).allowed, "no live connections");
  auto scope = baseline_fixture(); scope.scope = "other_repository";
  require(!check(scope).allowed, "scope isolation");
  auto role = baseline_fixture(); role.role = "viewer";
  require(!check(role).allowed, "role isolation");
  auto missing = baseline_fixture(); missing.gates.pop_back();
  require(!check(missing).allowed, "missing evidence gate");
  auto reordered = baseline_fixture(); std::swap(reordered.gates[0], reordered.gates[1]);
  require(!check(reordered).allowed, "gate order integrity");
  require(assess("unknown", "reviewer") == assess("unknown", "viewer"), "opaque unknown assessment");
  require(baseline("viewer") == assess("unknown", "viewer"), "uniform baseline denial");
  require(baseline("reviewer").find("allow_preview") != std::string::npos, "preview marker");
  require(report().find("\"real_repository_writes\":0") != std::string::npos, "not a live repo operation");
  require(list().find("untrusted_change_instruction") != std::string::npos, "inventory discovery");
  require(impact("reviewer","documented","documented","bounded","unknown_not_invented","reviewed","documented").find("review_required") != std::string::npos, "impact checklist");
  require(impact("reviewer","documented","documented","bounded","invented","reviewed","documented").find("deny") != std::string::npos, "fabricated impact denied");
  require(impact("reviewer","documented","unknown","bounded","unknown_not_invented","reviewed","documented").find("deny") != std::string::npos, "unknown consumers denied");
  require(impact("viewer","documented","documented","bounded","unknown_not_invented","reviewed","documented") == assess("unknown", "viewer"), "impact permission parity");
  std::cout << "Change policy: " << n << " assertions passed\n";
}
