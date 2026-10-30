#include "simulator/simulator.hpp"
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace simulator {
namespace {
bool valid_resource(std::string_view resource) {
  if (resource.empty() || resource.size() > 256) return false;
  return std::all_of(resource.begin(), resource.end(), [](unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_' || c == '.' ||
           c == '-' || c == ':' || c == '@';
  });
}
}  // namespace

const std::vector<Tool>& tools() {
  static const std::vector<Tool> available{
      {"websites.list", "websites"},
      {"websites.deploy_preview", "websites"},
      {"vps.list", "vps"},
      {"vps.power_state", "vps"},
      {"domains.list", "domains"},
      {"dns.records", "domains"},
      {"mail.domains", "mail"},
      {"mailboxes.list", "mail"},
      {"mailboxes.create_preview", "mail"},
      {"collaboration.calendar_preview", "collaboration"},
      {"collaboration.contacts_preview", "collaboration"},
      {"collaboration.tasks_preview", "collaboration"},
      {"subscriptions.list", "subscriptions"},
      {"ecommerce.store_preview", "ecommerce"},
      {"support.ticket_preview", "support"},
      {"backup.restore_preview", "backup"},
      {"monitoring.health", "monitoring"}};
  return available;
}

std::string escape_json(std::string_view input) {
  std::ostringstream out;
  for (unsigned char c : input) {
    switch (c) {
      case '"': out << "\\\""; break;
      case '\\': out << "\\\\"; break;
      case '\n': out << "\\n"; break;
      case '\r': out << "\\r"; break;
      case '\t': out << "\\t"; break;
      default:
        if (c < 0x20) {
          out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
              << static_cast<unsigned int>(c) << std::dec;
        } else {
          out << static_cast<char>(c);
        }
    }
  }
  return out.str();
}

std::string list_tools() {
  std::string result = "{\"mode\":\"simulation_only\",\"tools\":[";
  bool first = true;
  for (const auto& tool : tools()) {
    if (!first) result += ',';
    first = false;
    result += "{\"name\":\"" + escape_json(tool.name) +
              "\",\"domain\":\"" + escape_json(tool.domain) + "\"}";
  }
  result += "]}";
  return result;
}

std::string call_tool(std::string_view name, std::string_view resource) {
  if (!valid_resource(resource)) {
    return "{\"ok\":false,\"error\":\"invalid_resource\"}";
  }
  const auto& available = tools();
  const auto it = std::find_if(available.begin(), available.end(),
                               [name](const Tool& tool) { return tool.name == name; });
  if (it == available.end()) {
    return "{\"ok\":false,\"error\":\"unknown_tool\"}";
  }
  const std::string mode = name == "monitoring.health" ? "healthy" : "preview";
  return "{\"ok\":true,\"mode\":\"simulation_only\",\"tool\":\"" +
         escape_json(name) + "\",\"domain\":\"" + escape_json(it->domain) +
         "\",\"resource\":\"" + escape_json(resource) +
         "\",\"state\":\"" + mode + "\",\"side_effects\":false}";
}
}  // namespace simulator
