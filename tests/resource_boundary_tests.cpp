#include "simulator/simulator.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>

namespace {
int failures = 0;
void check(bool condition, std::string_view case_name) {
  if (!condition) {
    std::cerr << "FAIL: " << case_name << '\n';
    ++failures;
  }
}

// Independent reference grammar for identifier-only preview resources.
// ASCII only, 1..256 bytes, explicit finite alphabet, no leading/trailing
// dot or consecutive dots. It is not a pathname, URI, DNS or email parser.
bool expected_valid(std::string_view resource) {
  if (resource.empty() || resource.size() > 256) return false;
  for (std::size_t i = 0; i < resource.size(); ++i) {
    const auto c = static_cast<unsigned char>(resource[i]);
    const bool allowed = (c >= 'a' && c <= 'z') ||
                         (c >= 'A' && c <= 'Z') ||
                         (c >= '0' && c <= '9') || c == '_' ||
                         c == '.' || c == '-' || c == ':' || c == '@';
    if (!allowed) return false;
    if (c == '.' && (i == 0 || i + 1 == resource.size() ||
                      resource[i - 1] == '.')) return false;
  }
  return true;
}

bool accepted(std::string_view tool, std::string_view resource) {
  return simulator::call_tool(tool, resource).find("\"ok\":true") !=
         std::string::npos;
}

void test_allowlist() {
  const std::array<std::string, 11> valid{
      "demo", "a", "z", "A-Z", "a_1", "a.b", "mail@example.test",
      "namespace:resource", "VPS-0001", "A@b", std::string(256, 'a')};
  for (const auto& id : valid) {
    check(expected_valid(id), "reference accepts documented identifier");
    check(accepted("monitoring.health", id), "valid identifier is accepted");
  }
  const std::array<std::string, 21> invalid{
      "", ".", "..", "...", ".a", "a.", "a..b", "a...b",
      "a/.b", "../secrets", "a\\b", "a%2eb", "a b", "a\nb", "a\rb",
      "a\tb", "a\"b", "a'b", "a;b", std::string(257, 'a'),
      std::string("a\0b", 3)};
  for (const auto& id : invalid) {
    check(!expected_valid(id), "reference denies malformed identifier");
    check(!accepted("monitoring.health", id), "malformed identifier is denied");
  }
}

void test_every_byte_and_every_tool() {
  const auto& catalog = simulator::tools();
  check(catalog.size() == 17, "expected documented preview catalog");
  for (const auto& tool : catalog) {
    check(accepted(tool.name, "safe_fixture"), "catalog tool accepts safe resource");
    for (int n = 0; n < 256; ++n) {
      const auto c = static_cast<char>(n);
      std::string resource(1, c);
      const auto safe = expected_valid(resource);
      if (accepted(tool.name, resource) != safe) {
        std::cerr << "FAIL: byte classification mismatch (decimal " << n << ")\n";
        ++failures;
      }
    }
    check(!accepted(tool.name, "a..b"), "all tools reject ambiguous dot token");
    check(!accepted(tool.name, std::string(257, 'x')),
          "all tools enforce resource length");
  }
}

std::uint64_t next_random(std::uint64_t& value) {
  value ^= value << 13;
  value ^= value >> 7;
  value ^= value << 17;
  return value;
}

void test_deterministic_mutations() {
  std::uint64_t state = 0x4A1B2C3D4E5F6071ULL;
  for (int case_number = 0; case_number < 12000; ++case_number) {
    const auto len = static_cast<std::size_t>(next_random(state) % 275);
    std::string resource;
    resource.reserve(len);
    for (std::size_t i = 0; i < len; ++i) {
      resource.push_back(static_cast<char>(next_random(state) & 0xFF));
    }
    if (accepted("monitoring.health", resource) != expected_valid(resource)) {
      std::cerr << "FAIL: deterministic binary-input mutation " << case_number << '\n';
      ++failures;
    }
  }
}

void test_contract_non_disclosure() {
  const auto deny = simulator::call_tool("monitoring.health", "a..b");
  check(deny == R"({"ok":false,"error":"invalid_resource"})",
        "opaque invalid-resource denial");
  check(simulator::call_tool("missing.tool", "safe_fixture") ==
        R"({"ok":false,"error":"unknown_tool"})",
        "unknown tool denial remains stable");
  const auto allowed = simulator::call_tool("monitoring.health", "mail@example.test");
  check(allowed.find("\"mode\":\"simulation_only\"") != std::string::npos,
        "accepted request remains simulation-only");
  check(allowed.find("\"side_effects\":false") != std::string::npos,
        "accepted request does not claim network writes");
}
}

int main() {
  test_allowlist();
  test_every_byte_and_every_tool();
  test_deterministic_mutations();
  test_contract_non_disclosure();
  if (failures) {
    std::cerr << failures << " resource boundary checks failed\n";
    return 1;
  }
  std::cout << "Resource boundary regression passed: 17 tools, all byte values, "
               "12,000 deterministic mutations and documented identifier contracts.\n";
  return 0;
}