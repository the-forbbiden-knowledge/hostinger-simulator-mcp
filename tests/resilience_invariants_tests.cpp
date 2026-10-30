#include "simulator/simulator.hpp"
#include <atomic>
#include <chrono>
#include <iostream>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace {
int assertions = 0;
void require(bool condition, const char* label) {
  ++assertions;
  if (!condition) {
    std::cerr << "FAIL: " << label << '\n';
    std::exit(1);
  }
}
bool is_preview(const std::string& text) {
  return text.find("\"ok\":true") != std::string::npos &&
         text.find("\"mode\":\"simulation_only\"") != std::string::npos &&
         text.find("\"side_effects\":false") != std::string::npos;
}
}  // namespace

int main() {
  const auto& tools = simulator::tools();
  require(tools.size() == 17, "tool inventory invariant");
  std::set<std::string_view> names;
  for (const auto& tool : tools) {
    require(!tool.name.empty(), "tool name must exist");
    require(!tool.domain.empty(), "tool domain must exist");
    require(names.insert(tool.name).second, "tool names must be unique");
    const std::string first = simulator::call_tool(tool.name, "safe_fixture");
    require(is_preview(first), "read-only tool preview");
    require(first == simulator::call_tool(tool.name, "safe_fixture"), "deterministic tool response");
    require(first.find("\"resource\":\"safe_fixture\"") != std::string::npos, "resource echoed safely");
    require(first.size() <= 2048, "bounded normal response");
  }

  require(simulator::call_tool("nonexistent.action", "safe_fixture").find("\"ok\":false") !=
              std::string::npos, "unknown tools fail closed");
  const std::vector<std::string> invalid{
      "", "bad space", "bad\nline", "bad\tvalue", "bad\"quote",
      std::string(257, 'a'), "slash/path", "<script>", "unicode_\xC3\xA9"};
  for (const auto& input : invalid) {
    for (const auto& tool : tools) {
      const std::string response = simulator::call_tool(tool.name, input);
      require(response.find("\"ok\":false") != std::string::npos, "malformed resource must be denied");
      require(response.find("\"invalid_resource\"") != std::string::npos, "consistent invalid-resource classification");
    }
  }
  const std::string boundary(256, 'a');
  require(is_preview(simulator::call_tool("monitoring.health", boundary)), "maximum accepted input length");
  require(simulator::escape_json("\"\\\n\r\t") == "\\\"\\\\\\n\\r\\t", "JSON escape invariant");
  require(simulator::escape_json(std::string("\x01", 1)) == "\\u0001", "control character escaping");

  constexpr int workers = 8;
  constexpr int rounds = 250;
  std::atomic<bool> pass{true};
  std::vector<std::thread> pool;
  pool.reserve(workers);
  const auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < workers; ++i) {
    pool.emplace_back([&, i] {
      for (int round = 0; round < rounds; ++round) {
        const auto& tool = tools[static_cast<std::size_t>((round + i) % tools.size())];
        const std::string valid = simulator::call_tool(tool.name, "isolation_test");
        const std::string denied = simulator::call_tool(tool.name, "unsafe resource");
        if (!is_preview(valid) || denied.find("\"ok\":false") == std::string::npos ||
            valid.find("\"resource\":\"isolation_test\"") == std::string::npos) {
          pass.store(false, std::memory_order_relaxed);
          return;
        }
      }
    });
  }
  for (auto& worker : pool) worker.join();
  require(pass.load(), "concurrent valid/invalid calls are isolated");
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start);
  std::cout << "Resilience invariants: " << assertions
            << " assertions; concurrent operations=" << workers * rounds * 2
            << "; elapsed_ms=" << elapsed.count() << '\n';
}
