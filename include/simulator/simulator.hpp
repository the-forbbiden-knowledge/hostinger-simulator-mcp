#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace simulator {
struct Tool { std::string_view name; std::string_view domain; };
const std::vector<Tool>& tools();
std::string escape_json(std::string_view input);
std::string list_tools();
std::string call_tool(std::string_view name, std::string_view resource);
}
