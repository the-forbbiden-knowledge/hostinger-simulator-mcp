#pragma once
#include <string>
#include <string_view>
#include <vector>
namespace simulator::assessment_lifecycle {
struct PhaseTransition { std::string_view current, next, required_evidence; };
struct Scenario { std::string_view id, expected_gate; };
struct Decision { bool allowed; std::string_view failed_gate; };
const std::vector<PhaseTransition>& transitions();
const std::vector<Scenario>& scenarios();
Decision check(std::string_view role, std::string_view tenant, std::string_view current,
               std::string_view next, std::string_view evidence,
               std::string_view approval, std::string_view telemetry) noexcept;
std::string list();
std::string report();
std::string baseline(std::string_view role);
std::string simulate(std::string_view id, std::string_view role);
std::string advance(std::string_view role, std::string_view tenant, std::string_view current,
                    std::string_view next, std::string_view evidence,
                    std::string_view approval, std::string_view telemetry);
}  // namespace simulator::assessment_lifecycle
