#include "simulator/zimbra_security.hpp"
#include <cstdlib>
#include <iostream>
#include <set>
#include <string_view>
using namespace simulator::zimbra_security;
int main() {
  int checks = 0;
  auto require = [&checks](bool ok, const char* label) {
    ++checks;
    if (!ok) { std::cerr << "FAIL: " << label << '\n'; std::exit(1); }
  };
  require(scenarios().size() == 9, "nine synthetic cases");
  std::set<std::string_view> ids;
  for (const auto& item : scenarios()) {
    require(ids.insert(item.id).second, "unique case identifier");
    require(!item.failed_gate.empty(), "identified policy gate");
    auto response = simulate(item.id, "reviewer");
    require(response.find("\"decision\":\"no_go\"") != std::string::npos, "fixture denied");
    require(response.find(std::string(item.failed_gate)) != std::string::npos, "gate attributed");
    require(response.find("\"network_access\":false") != std::string::npos, "no network");
    require(assess(item.id, "viewer") == assess("invalid", "viewer"), "opaque unauthorized");
  }
  require(evaluate_policy("reviewer", "hosted", "same_tenant", "health_preview", "approved_read_only").allowed, "hosted health");
  require(evaluate_policy("reviewer", "hosted", "same_tenant", "restore_preview", "approved_read_only").allowed, "hosted restore preview");
  require(!evaluate_policy("reviewer", "hosted", "same_tenant", "end_user_support_preview", "approved_read_only").allowed, "hosted support limited");
  require(!evaluate_policy("reviewer", "hosted", "same_tenant", "account_change_preview", "approved_read_only").allowed, "hosted admin limited");
  require(evaluate_policy("reviewer", "managed", "same_tenant", "end_user_support_preview", "approved_read_only").allowed, "managed helpdesk");
  require(evaluate_policy("reviewer", "managed", "same_tenant", "account_change_preview", "approved_read_only").allowed, "managed admin preview");
  require(!evaluate_policy("reviewer", "managed", "other_tenant", "restore_preview", "approved_read_only").allowed, "tenant isolation");
  require(!evaluate_policy("reviewer", "managed", "same_tenant", "restore_preview", "absent").allowed, "approval required");
  require(!evaluate_policy("viewer", "managed", "same_tenant", "health_preview", "approved_read_only").allowed, "role restriction");
  require(!evaluate_policy("reviewer", "managed", "same_tenant", "send_mail", "approved_read_only").allowed, "no mail delivery");
  require(!evaluate_policy("reviewer", "managed", "same_tenant", "claim_sla_credit", "approved_read_only").allowed, "no financial effects");
  require(evaluate("viewer", "managed", "same_tenant", "health_preview", "approved_read_only") == assess("unknown", "viewer"), "uniform denial");
  require(evaluate("reviewer", "managed", "same_tenant", "health_preview", "approved_read_only").find("allow_preview") != std::string::npos, "preview only");
  require(baseline("reviewer").find("allow_preview") != std::string::npos, "baseline");
  require(report().find("\"real_zimbra_connections\":0") != std::string::npos, "no live provider claims");
  require(list().find("hosted_end_user_helpdesk") != std::string::npos, "inventory");
  std::cout << "Zimbra offline policy: " << checks << " assertions passed\n";
}
