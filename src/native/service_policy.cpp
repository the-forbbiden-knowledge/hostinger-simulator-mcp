#include "simulator/service_policy.hpp"

namespace simulator::service_policy {
namespace {
std::string deny() {
  return R"({"ok":false,"decision":"deny","error":"not_available_or_authorized","mode":"simulation_only","network_access":false,"side_effects":false})";
}
std::string allow(std::string_view plan, std::string_view action) {
  return std::string(R"({"ok":true,"decision":"allow_preview","service":"service_collaboration_fixture","plan":")") +
         std::string(plan) + R"(","action":")" + std::string(action) +
         R"(","source_status":"historical_reference_only","mode":"simulation_only","network_access":false,"side_effects":false})";
}
}  // namespace

const std::vector<Scenario>& scenarios() {
  static const std::vector<Scenario> cases{
      {"hosted_end_user_helpdesk", "support", "plan_scope"},
      {"hosted_account_changes", "administration", "plan_scope"},
      {"cross_tenant_restore", "backup", "tenant"},
      {"unapproved_restore", "backup", "approval"},
      {"external_mail_delivery", "mail", "offline_only"},
      {"unverified_uptime_guarantee", "sla", "historical_terms"},
      {"automatic_sla_credit", "billing", "historical_terms"},
      {"mail_message_instruction", "agent", "trusted_authority"},
      {"private_mail_content", "privacy", "data_minimization"},
  };
  return cases;
}

Decision evaluate_policy(std::string_view role, std::string_view plan,
                         std::string_view tenant, std::string_view action,
                         std::string_view approval) noexcept {
  if (role != "reviewer") return {false, "role"};
  if (tenant != "same_tenant") return {false, "tenant"};
  if (approval != "approved_read_only") return {false, "approval"};
  if (plan != "hosted" && plan != "managed") return {false, "plan"};
  if (action == "health_preview" || action == "admin_support_preview" ||
      action == "restore_preview" || action == "collaboration_preview") {
    return {true, "none"};
  }
  if (action == "end_user_support_preview" || action == "account_change_preview") {
    if (plan == "managed") return {true, "none"};
    return {false, "plan_scope"};
  }
  // No operation may send mail, alter accounts, initiate restores, or commit credits.
  return {false, "offline_only"};
}

std::string list() {
  std::string result = R"({"ok":true,"mode":"simulation_only","network_access":false,"scenarios":[)";
  bool first = true;
  for (const auto& item : scenarios()) {
    if (!first) result += ',';
    first = false;
    result += R"({"id":")" + std::string(item.id) + R"(","domain":")" +
              std::string(item.domain) + R"("})";
  }
  return result + "]}";
}
std::string report() {
  return R"({"ok":true,"mode":"simulation_only","scenario_count":9,"source_status":"historical_reference_only","plans":["hosted","managed"],"real_service_connections":0,"real_tickets":0,"live_mailboxes":0,"verified_sla_guarantees":0,"network_access":false,"side_effects":false})";
}
std::string baseline(std::string_view role) {
  const auto result = evaluate_policy(role, "managed", "same_tenant",
                                      "health_preview", "approved_read_only");
  return result.allowed ? allow("managed", "health_preview") : deny();
}
std::string assess(std::string_view id, std::string_view role) {
  if (role != "reviewer") return deny();
  for (const auto& item : scenarios()) {
    if (item.id == id) {
      return R"({"ok":true,"decision":"review_required","evidence":"synthetic_only","mode":"simulation_only","network_access":false,"side_effects":false})";
    }
  }
  return deny();
}
std::string simulate(std::string_view id, std::string_view role) {
  if (role != "reviewer") return deny();
  for (const auto& item : scenarios()) {
    if (item.id == id) {
      return std::string(R"({"ok":true,"decision":"no_go","failed_gate":")") +
             std::string(item.failed_gate) +
             R"(","evidence":"synthetic_only","mode":"simulation_only","network_access":false,"side_effects":false})";
    }
  }
  return deny();
}
std::string evaluate(std::string_view role, std::string_view plan,
                     std::string_view tenant, std::string_view action,
                     std::string_view approval) {
  return evaluate_policy(role, plan, tenant, action, approval).allowed
             ? allow(plan, action)
             : deny();
}
}  // namespace simulator::service_policy
