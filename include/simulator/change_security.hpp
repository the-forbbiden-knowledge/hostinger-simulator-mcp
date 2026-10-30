#pragma once
#include <string>
#include <string_view>
#include <vector>
namespace simulator::change_security {
struct Scenario { std::string_view id, domain, failed_gate; };
struct Gate { std::string_view name, value; };
struct Evidence {
  std::string_view role{"reviewer"};
  std::string_view scope{"approved_fixture"};
  bool offline{true};
  std::vector<Gate> gates;
};
struct Decision { bool allowed; std::string_view failed_gate; };
const std::vector<Scenario>& scenarios();
Evidence baseline_fixture();
Evidence bad_fixture(std::string_view gate);
Decision check(const Evidence&) noexcept;
std::string list();
std::string report();
std::string baseline(std::string_view role);
std::string assess(std::string_view id, std::string_view role);
std::string simulate(std::string_view id, std::string_view role);
std::string impact(std::string_view role, std::string_view decisions,
                   std::string_view consumers, std::string_view volume,
                   std::string_view financial, std::string_view regulatory,
                   std::string_view recovery);
}  // namespace simulator::change_security
