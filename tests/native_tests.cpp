#include "simulator/simulator.hpp"
#include <iostream>
#include <string>
#include <string_view>

namespace {
int failures = 0;
void check(bool condition, std::string_view description) {
  if (!condition) {
    std::cerr << "FAIL: " << description << '\n';
    ++failures;
  }
}
}

int main() {
  check(simulator::tools().size() == 17, "exactly 17 documented tools");
  check(simulator::list_tools().find("monitoring.health") != std::string::npos,
        "tool discovery includes health");
  const auto result = simulator::call_tool("monitoring.health", "demo");
  check(result.find("\"ok\":true") != std::string::npos, "known tool succeeds");
  check(result.find("\"side_effects\":false") != std::string::npos,
        "response explicitly marks simulation-only behavior");
  check(simulator::call_tool("monitoring.health", "demo") == result,
        "identical inputs produce identical results");
  check(simulator::call_tool("unknown.operation", "demo").find("unknown_tool") != std::string::npos,
        "unknown tool denied");
  check(simulator::call_tool("vps.list", "").find("invalid_resource") != std::string::npos,
        "empty resource denied");
  check(simulator::call_tool("vps.list", "a\nb").find("invalid_resource") != std::string::npos,
        "control character denied");
  check(simulator::call_tool("vps.list", "../private/path").find("invalid_resource") != std::string::npos,
        "path-shaped resource denied");
  check(simulator::call_tool("vps.list", "a\"b").find("invalid_resource") != std::string::npos,
        "quote-shaped resource denied");
  check(simulator::call_tool("vps.list", std::string(257, 'a')).find("invalid_resource") != std::string::npos,
        "oversized resource denied");
  check(simulator::call_tool("vps.list", "mail@example.test").find("\"ok\":true") != std::string::npos,
        "mailbox-style safe ID accepted");
  check(simulator::escape_json("\"\\\n") == "\\\"\\\\\\n",
        "JSON escaping is stable");
  if (failures != 0) {
    std::cerr << failures << " test assertion(s) failed\n";
    return 1;
  }
  std::cout << "Native validation passed (13 checks).\n";
  return 0;
}
