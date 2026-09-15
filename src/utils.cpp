#include "../includes/utils.hpp"
#include <sstream>
#include <string>
#include <vector>
namespace logicsim {
std::string gname(int tp) {
  switch (tp) {
  case 0:
    return ("PI");
  case 1:
    return ("BRANCH");
  case 2:
    return ("XOR");
  case 3:
    return ("OR");
  case 4:
    return ("NOR");
  case 5:
    return ("NOT");
  case 6:
    return ("NAND");
  case 7:
    return ("AND");
  case 8:
    return ("XNOR");
  case 9:
    return ("BUF");
  }
  return "";
}

std::string trimCopy(const std::string &input) {
  std::size_t first = input.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return "";
  }
  std::size_t last = input.find_last_not_of(" \t\r\n");
  return input.substr(first, last - first + 1);
}

std::vector<std::string> parseCommandArgs(std::string rawArgs) {
  std::vector<std::string> parsedArgs;
  if (rawArgs.empty()) {
    return parsedArgs;
  }

  std::string args = rawArgs;
  auto newline = args.find_first_of("\r\n");
  if (newline != std::string::npos) {
    args.erase(newline);
  }

  std::istringstream argStream(args);
  std::string token;
  while (argStream >> token) {
    parsedArgs.push_back(std::move(token));
  }

  return parsedArgs;
}
LogicValue toLogicValue(char c) {
  switch (c) {
  case '0':
    return LogicValue::ZERO;
  case '1':
    return LogicValue::ONE;
  case 'x':
    return LogicValue::UNDEF;
  default:
    return LogicValue::UNDEF;
  }
}
} // namespace logicsim
