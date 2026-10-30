#include "simulator/recon_security.hpp"
#include <iostream>
#include <set>
#include <string>
#include <string_view>

namespace {
int failures = 0;
int checks = 0;
void check(bool ok, std::string_view why) {
  ++checks;
  if (!ok) { ++failures; std::cerr << "FAIL: " << why << '\n'; }
}
bool has(std::string_view text, std::string_view needle) {
  return text.find(needle) != std::string_view::npos;
}
}
int main() {
  using simulator::recon_security::inspect;
  const auto& scenarios = simulator::recon_security::scenarios();
  std::set<std::string_view> names;
  check(scenarios.size() == 20, "20 synthetic exposure cases catalogued");
  for (const auto& scenario : scenarios) {
    check(names.insert(scenario.id).second, "case identifiers unique");
    check(!scenario.layer.empty() && !scenario.required_control.empty(), "every case mapped to layer and control");
    const auto reviewer = inspect(scenario.id, "reviewer");
    check(has(reviewer, "\"decision\":\"allow_preview\""), "reviewer gets synthetic summary");
    check(has(reviewer, "\"side_effects\":false") && has(reviewer, "\"network_access\":false"), "no external side effects");
    check(!has(reviewer, "API_KEY") && !has(reviewer, "password") && !has(reviewer, "http://"), "no secret or endpoint material");
    const auto viewer = inspect(scenario.id, "viewer");
    check(has(viewer, scenario.restricted ? "\"decision\":\"deny\"" : "\"decision\":\"allow_preview\""), "viewer redaction boundary");
    check(inspect(scenario.id, "operator") == viewer, "operators cannot bypass reviewer-only disclosure");
    check(inspect(scenario.id, "reviewer") == reviewer, "deterministic preview");
  }
  const auto unknown = inspect("not_a_fixture", "viewer");
  check(unknown == inspect("rag_source_attribution", "viewer"), "unknown and unauthorized indistinguishable");
  check(unknown == inspect("rag_source_attribution", "elevated"), "invalid role denied uniformly");
  check(unknown == inspect("", "reviewer"), "empty ID rejected");
  check(unknown == inspect("mcp_tool_schema_visibility", "viewer"), "protected tool schema denied");
  check(has(simulator::recon_security::list_scenarios(), "\"network_access\":false"), "inventory is offline only");
  check(has(simulator::recon_security::list_scenarios(), "canary_response_integrity"), "canary case included");
  check(!failures, "all control checks passed");
  if (failures) { std::cerr << failures << "/" << checks << " tests failed\n"; return 1; }
  std::cout << "Recon inventory: " << checks << " assertions passed\n";
  return 0;
}
