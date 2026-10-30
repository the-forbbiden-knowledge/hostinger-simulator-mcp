#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace simulator::capstone_security {
struct Scenario {
  std::string_view id;
  std::string_view boundary;
  std::string_view risk;
  std::string_view required_control;
};
const std::vector<Scenario>& scenarios();
std::string list_scenarios();
std::string assess(std::string_view scenario_id, std::string_view role);
std::string simulate(std::string_view scenario_id, std::string_view source,
                     std::string_view role, std::string_view session_scope,
                     std::string_view approval);
std::string report();
} // namespace simulator::capstone_security
