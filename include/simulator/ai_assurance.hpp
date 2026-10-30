#pragma once
#include <string>
#include <string_view>
#include <vector>
namespace simulator::ai_assurance {
struct Scenario {std::string_view id, domain, risk, control, failing_gate;};
struct Gate {std::string_view name, value;};
struct Evidence {std::string_view role{"reviewer"};std::string_view scope{"approved_fixture"};std::string_view session{"same_session"};bool offline{true};std::vector<Gate> gates;};
struct Decision {bool allowed; std::string_view failed_gate;};
const std::vector<Scenario>& scenarios();
Evidence baseline_fixture();
Evidence bad_fixture(std::string_view gate);
Decision check(const Evidence&) noexcept;
std::string list();
std::string report();
std::string baseline(std::string_view role);
std::string assess(std::string_view id,std::string_view role);
std::string simulate(std::string_view id,std::string_view role);
std::string decide(std::string_view role,std::string_view scope,std::string_view evidence,std::string_view approval,std::string_view telemetry);
std::string impact(std::string_view role,std::string_view scope,std::string_view volume,std::string_view consumer,std::string_view financial);
} // namespace simulator::ai_assurance
