#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace simulator::recon_security {
struct Scenario {
  std::string_view id;
  std::string_view layer;
  std::string_view risk;
  std::string_view required_control;
  bool restricted;
};
const std::vector<Scenario>& scenarios();
std::string list_scenarios();
std::string inspect(std::string_view scenario_id, std::string_view role);
} // namespace simulator::recon_security
