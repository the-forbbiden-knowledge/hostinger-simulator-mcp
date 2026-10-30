#include "simulator/agent_security.hpp"
#include "simulator/simulator.hpp"

#include <array>
#include <iostream>
#include <string>
#include <string_view>

namespace {
int failures = 0;
std::size_t checks = 0;
void require(bool ok, std::string_view label) {
  ++checks;
  if (!ok) {
    ++failures;
    if (failures <= 10) std::cerr << "FAIL: " << label << '\n';
  }
}
bool contains(std::string_view value, std::string_view fragment) {
  return value.find(fragment) != std::string_view::npos;
}
} // namespace

int main() {
  using simulator::agent_security::guarded_call;
  constexpr std::array<std::string_view, 9> sources{{
      "trusted_application", "user_input", "document", "document_batch",
      "web_page", "source_code", "tool_output", "shared_memory", "private_memory"}};
  constexpr std::array<std::string_view, 3> roles{{"viewer", "operator", "reviewer"}};
  constexpr std::array<std::string_view, 2> sessions{{"same_session", "cross_session"}};
  constexpr std::array<std::string_view, 2> approvals{{"none", "approved"}};
  require(simulator::tools().size() == 17, "expected native tool inventory");

  // Check every combination, at every tool boundary: 17 * 9 * 3 * 2 * 2.
  for (const auto& tool : simulator::tools()) {
    for (const auto source : sources) {
      for (const auto role : roles) {
        for (const auto session : sessions) {
          for (const auto approval : approvals) {
            const bool authorized = source == "trusted_application" &&
                role != "viewer" && session == "same_session" &&
                approval == "approved";
            const auto response = guarded_call(
                source, role, session, approval, tool.name, "fixture_1");
            if (authorized) {
              require(contains(response, "\"ok\":true") &&
                      contains(response, "\"tool\":\"" + std::string(tool.name) + "\"") &&
                      contains(response, "\"mode\":\"simulation_only\"") &&
                      contains(response, "\"side_effects\":false"),
                      "authorized preview remains local, typed, and bounded");
            } else {
              require(contains(response, "\"decision\":\"deny\"") &&
                      !contains(response, "\"tool\":") &&
                      !contains(response, "\"resource\":"),
                      "untrusted or unauthorized input cannot reach native dispatch");
            }
          }
        }
      }
    }
  }

  const std::array<std::string, 9> invalid_resources{{
    "", ".", "..", ".fixture", "fixture.", "a..b", "../private",
    "a/b", std::string("a\0b", 3)}};
  for (const auto& tool : simulator::tools()) {
    for (const auto& resource : invalid_resources) {
      const auto response = guarded_call(
          "trusted_application", "operator", "same_session", "approved",
          tool.name, resource);
      require(contains(response, "\"error\":\"invalid_resource\""),
              "native resource validation remains mandatory after policy approval");
    }
  }
  const auto invalid_tool = guarded_call(
      "trusted_application", "operator", "same_session", "approved",
      "unknown.fixture", "fixture_1");
  require(contains(invalid_tool, "\"error\":\"unknown_tool\""),
          "native registry remains independent allowlist");
  const auto untrusted_unknown_tool = guarded_call(
      "tool_output", "reviewer", "same_session", "approved",
      "unknown.fixture", "fixture_1");
  require(contains(untrusted_unknown_tool, "\"decision\":\"deny\"") &&
          !contains(untrusted_unknown_tool, "\"error\":\"unknown_tool\""),
          "policy decision precedes native tool-name lookup");
  const auto bad_role = guarded_call(
      "trusted_application", "administrator", "same_session", "approved",
      "monitoring.health", "fixture_1");
  require(contains(bad_role, "\"reason\":\"invalid_policy_input\""),
          "unknown role denied before dispatch");
  if (failures) {
    std::cerr << failures << " failed of " << checks << " matrix checks\n";
    return 1;
  }
  std::cout << "Complete mediation: " << checks
            << " synthetic native policy assertions passed\n";
  return 0;
}
