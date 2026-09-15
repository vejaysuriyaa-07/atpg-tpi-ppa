#pragma once

#include <string>
#include <vector>

namespace logicsim {
class Simulator;

enum class RtpgMode { V0, V1, V2, V3, V4 };
enum class AtpgAlgo { DALG, PODEM };
enum class DalgDf { NL, NH, LH, CC };
enum class JfSetting { V0, V1 };

struct RtpgOptions {
  RtpgMode mode = RtpgMode::V0;
  std::string param1; // Stored as string, parse as needed (int, double, etc.)
  std::string param2; // Stored as string, parse as needed (int, double, etc.)
};

struct AtpgOptions {
  AtpgAlgo algo = AtpgAlgo::DALG;
  DalgDf df = DalgDf::NL; // Only meaningful for DALG
  JfSetting jf = JfSetting::V0;
};

struct TpgOptions {
  RtpgOptions rtpg;
  AtpgOptions atpg;
  std::string outputPath;
  bool hasOutput = false;
};

bool parseTpgOptions(const std::vector<std::string> &tokens,
                     TpgOptions &options, std::string &error);
std::vector<std::vector<unsigned>>
testPatternGenerator_impl(Simulator &simulator);
} // namespace logicsim
