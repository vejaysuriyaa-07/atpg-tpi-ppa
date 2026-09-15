#include <chrono>
#include "../includes/core/rtpg.hpp"
#include "../includes/Simulator.hpp"
#include <sstream>
#include <fstream>
#include <random>
namespace logicsim {
    std::vector<std::string> parseCommandArgs(Simulator &simulator) {
        auto &rawArgs = simulator.getCommandArgs();
        if (rawArgs.empty()) {
            printf("RTPG missing arguments\n");
            return {};
        }

        std::string args = rawArgs;
        auto newline = args.find_first_of("\r\n");
        if (newline != std::string::npos) {
            args.erase(newline);
        }

        std::istringstream argStream(args);
        std::string tpCount;
        std::string mode;
        std::string outputPath;
        if (!(argStream >> tpCount >> mode >> outputPath)) {
            printf("RTPG usage: RTPG <tp_count> <mode> <output_file>\n");
            return {};
        }
        std::string extra;
        if (argStream >> extra) {
            printf("RTPG expects exactly three arguments.\n");
            return {};
        }
        printf("RTPG: Generating %s test patterns in mode '%s' to %s\n", tpCount.c_str(), mode.c_str(), outputPath.c_str());

        return {std::move(tpCount), std::move(mode), std::move(outputPath)};
    }
    std::vector<std::vector<unsigned>> randomTestPatternGen_impl(Simulator &simulator, bool atpg, unsigned tpCount_arg, std::string mode_arg) {

        auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        static thread_local std::mt19937 rng(static_cast<unsigned>(now));

        
        std::string tpCount = std::to_string(tpCount_arg);
        std::string mode = mode_arg;
        std::string outputPath;
       
        if (!atpg) {
            auto args = parseCommandArgs(simulator); 
            if (args.empty()) {
                return {};
            }
            tpCount = args[0];
            mode = args[1];
            outputPath = args[2];
        }

        unsigned numPI = simulator.getNPI();
        std::vector<unsigned> headers;
        std::vector<std::vector<unsigned>> patterns;
        for (auto* pi: simulator.getPrimaryInputs()) {
            headers.push_back(pi->getNum());
        }
        for (int i = 0; i < std::stoi(tpCount); ++i) {
            patterns.push_back(std::vector<unsigned>{});
            for (int j = 0; j < numPI; ++j) {
                if (mode == "b") {
                    std::uniform_int_distribution<int> dist(0, 1);
                    std::array<LogicValue, 2> vals = {ZERO, ONE};
                    char val = vals[dist(rng)];
                    patterns[i].push_back(val);
                } else if (mode == "t") {
                    std::uniform_int_distribution<int> dist(0, 2);
                    std::array<LogicValue, 3> vals = {ZERO, ONE, UNDEF};
                    char val = vals[dist(rng)];
                    patterns[i].push_back(val);
                } else {
                    printf("Unsupported RTPG mode: %s\n", mode.c_str());
                    return {};
                }
            }
        }
        if (!atpg) {
            std::ofstream outfile;
            outfile.open(outputPath);
            bool first = true;
            for (const auto& h : headers) {
                if (!first) outfile << ',';
                first = false;
                outfile << h;
            }
            outfile << '\n';
            for (const auto& row : patterns) {
                first = true;
                for (const auto& val : row) {
                    if (!first) outfile << ',';
                    first = false;
                    outfile << ((val == 2) ? 'x' : static_cast<char>('0' + val));
                }
                outfile << '\n';
            }
            outfile.close();
        }
        patterns.insert(patterns.begin(), headers);
        return patterns;
    }
}