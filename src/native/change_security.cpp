#include "simulator/change_security.hpp"
#include <array>
#include <string>
#include <vector>
namespace simulator::change_security {
namespace {
constexpr std::array<std::string_view, 12> gate_names{{
    "origin", "evidence", "patch_review", "test_results",
    "secrets", "consumers", "transaction_volume", "regulatory",
    "financial", "human_approval", "rollback", "action"}};
constexpr std::array<std::string_view, 12> safe_values{{
    "trusted_application", "corroborated", "reviewed_diff", "passed",
    "redacted", "documented", "bounded", "reviewed",
    "unknown_not_invented", "approved", "documented", "read_only_preview"}};
std::string deny() {
  return R"({"ok":false,"decision":"deny","error":"not_available_or_authorized","mode":"simulation_only","network_access":false,"side_effects":false})";
}
} // namespace

const std::vector<Scenario>& scenarios() {
  static const std::vector<Scenario> cases{
      {"untrusted_change_instruction", "agent_input", "origin"},
      {"unverified_patch_justification", "evidence", "evidence"},
      {"unreviewed_code_diff", "change_review", "patch_review"},
      {"missing_test_results", "verification", "test_results"},
      {"private_data_in_patch", "privacy", "secrets"},
      {"unknown_downstream_consumers", "impact", "consumers"},
      {"unbounded_action_fanout", "impact", "transaction_volume"},
      {"regulatory_context_missing", "risk", "regulatory"},
      {"unsupported_loss_estimate", "risk", "financial"},
      {"autonomous_commit_without_approval", "authority", "human_approval"},
      {"no_backout_plan", "recovery", "rollback"},
      {"unapproved_remote_push", "action", "action"}};
  return cases;
}
Evidence baseline_fixture() {
  Evidence e;
  e.gates.reserve(gate_names.size());
  for (std::size_t i = 0; i < gate_names.size(); ++i)
    e.gates.push_back({gate_names[i], safe_values[i]});
  return e;
}
Evidence bad_fixture(std::string_view gate) {
  auto e = baseline_fixture();
  for (auto& item : e.gates)
    if (item.name == gate) item.value = "not_verified";
  if (gate == "offline") e.offline = false;
  return e;
}
Decision check(const Evidence& e) noexcept {
  if (e.role != "reviewer") return {false, "role"};
  if (e.scope != "approved_fixture") return {false, "scope"};
  if (!e.offline) return {false, "offline"};
  if (e.gates.size() != gate_names.size()) return {false, "shape"};
  for (std::size_t i = 0; i < gate_names.size(); ++i) {
    if (e.gates[i].name != gate_names[i] ||
        e.gates[i].value != safe_values[i]) return {false, gate_names[i]};
  }
  return {true, "none"};
}
std::string list() {
  std::string out = R"({"ok":true,"mode":"simulation_only","network_access":false,"scenarios":[)";
  bool first = true;
  for (const auto& item : scenarios()) {
    if (!first) out += ',';
    first = false;
    out += R"({"id":")" + std::string(item.id) +
           R"(","domain":")" + std::string(item.domain) + R"("})";
  }
  return out + "]}";
}
std::string report() {
  return R"({"ok":true,"mode":"simulation_only","scenario_count":12,"policy_gates":12,"actual_findings":0,"actual_financial_loss":null,"real_repository_writes":0,"network_access":false,"side_effects":false})";
}
std::string baseline(std::string_view role) {
  if (role != "reviewer") return deny();
  return check(baseline_fixture()).allowed
      ? R"({"ok":true,"decision":"allow_preview","evidence":"synthetic_only","mode":"simulation_only","network_access":false,"side_effects":false})"
      : deny();
}
std::string assess(std::string_view id, std::string_view role) {
  if (role != "reviewer") return deny();
  for (const auto& item : scenarios()) {
    if (item.id == id)
      return R"({"ok":true,"decision":"review_required","evidence":"synthetic_only","mode":"simulation_only","network_access":false,"side_effects":false})";
  }
  return deny();
}
std::string simulate(std::string_view id, std::string_view role) {
  if (role != "reviewer") return deny();
  for (const auto& item : scenarios()) {
    if (item.id != id) continue;
    const auto result = check(bad_fixture(item.failed_gate));
    if (result.allowed || result.failed_gate != item.failed_gate) return deny();
    return std::string(R"({"ok":true,"decision":"no_go","failed_gate":")") +
           std::string(result.failed_gate) +
           R"(","evidence":"synthetic_only","mode":"simulation_only","network_access":false,"side_effects":false})";
  }
  return deny();
}
std::string impact(std::string_view role, std::string_view decisions,
                   std::string_view consumers, std::string_view volume,
                   std::string_view financial, std::string_view regulatory,
                   std::string_view recovery) {
  if (role != "reviewer" || decisions != "documented" ||
      consumers != "documented" || volume != "bounded" ||
      financial != "unknown_not_invented" || regulatory != "reviewed" ||
      recovery != "documented") return deny();
  return R"({"ok":true,"decision":"review_required","impact_questions":6,"verified_loss":null,"actual_findings":0,"mode":"simulation_only","network_access":false,"side_effects":false})";
}
} // namespace simulator::change_security
