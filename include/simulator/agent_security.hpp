#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace simulator::agent_security {
struct Scenario { std::string_view id, entry_point, required_control; };
const std::vector<Scenario>& scenarios();
std::string list_scenarios();
std::string evaluate(std::string_view source, std::string_view action,
                     std::string_view role, std::string_view session_scope,
                     std::string_view approval);
std::string guarded_call(std::string_view source, std::string_view role,
                         std::string_view session_scope, std::string_view approval,
                         std::string_view tool, std::string_view resource);
} // namespace simulator::agent_security
