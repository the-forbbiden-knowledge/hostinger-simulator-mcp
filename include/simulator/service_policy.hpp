#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace simulator::service_policy {
struct Scenario {
  std::string_view id;
  std::string_view domain;
  std::string_view failed_gate;
};
struct Decision {
  bool allowed;
  std::string_view failed_gate;
};
const std::vector<Scenario>& scenarios();
Decision evaluate_policy(std::string_view role, std::string_view plan,
                         std::string_view tenant, std::string_view action,
                         std::string_view approval) noexcept;
std::string list();
std::string report();
std::string baseline(std::string_view role);
std::string assess(std::string_view scenario, std::string_view role);
std::string simulate(std::string_view scenario, std::string_view role);
std::string evaluate(std::string_view role, std::string_view plan,
                     std::string_view tenant, std::string_view action,
                     std::string_view approval);
}  // namespace simulator::service_policy
