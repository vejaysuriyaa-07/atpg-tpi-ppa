#include "includes/Simulator.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

#ifdef USE_READLINE
#include <readline/history.h>
#include <readline/readline.h>
#endif

using namespace logicsim;

#define Upcase(x) ((isalpha(x) && islower(x)) ? toupper(x) : (x))
#define Lowcase(x) ((isalpha(x) && isupper(x)) ? tolower(x) : (x))

int main() {
  Simulator sim;
  while (!sim.isDone()) {
    std::string line;

#ifdef USE_READLINE
    // Use readline for line editing support (arrow keys, history, etc.)
    char *line_c = readline("\nCommand>");
    if (!line_c)
      break; // EOF or error
    line = line_c;
    // Add non-empty lines to history
    if (!line.empty()) {
      add_history(line_c);
    }
    free(line_c);
#else
    // Fallback to basic input
    std::cout << "\nCommand>";
    if (!std::getline(std::cin, line))
      break;
#endif

    if (line.empty())
      continue;

    std::istringstream iss(line);
    std::string cmdToken;
    if (!(iss >> cmdToken))
      continue;
    for (char &ch : cmdToken) {
      ch = Upcase(ch);
    }

    std::string args;
    std::getline(iss, args);
    if (!args.empty()) {
      std::size_t first = args.find_first_not_of(" \t");
      if (first != std::string::npos) {
        args = args.substr(first);
      } else {
        args.clear();
      }
    }

    sim.setCommandArgs(args);

    int com = logicsim::READ;
    while (com < NUMFUNCS &&
           std::strcmp(cmdToken.c_str(), sim.getCommands()[com].getName()) != 0)
      com++;
    if (com < NUMFUNCS) {
      if (sim.getCommands()[com].getState() <= sim.getGlobalState())
        sim.getCommands()[com].invoke();
      else
        std::printf("Execution out of sequence!\n");
    } else
      std::system(line.c_str());
  }
}
