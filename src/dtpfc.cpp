#include "../includes/core/dtpfc.hpp"
#include "../includes/Simulator.hpp"
#include "../includes/utils.hpp"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unordered_set>
#include <utility>
#include <vector>
namespace logicsim {
int deterministicTestPatternCoverage_impl(Simulator &simulator) {
  // Start timing
  auto startTime = std::chrono::high_resolution_clock::now();

  assert(!simulator.getCommandArgs().empty());

  std::string args = simulator.getCommandArgs();
  auto newline = args.find_first_of("\r\n");
  if (newline != std::string::npos) {
    args.erase(newline);
  }

  std::istringstream argStream(args);
  std::string tpInputFile; // Input file containing test patterns
  int freq;
  std::string outputPath;
  std::string reportFilePath;
  if (!(argStream >> tpInputFile >> freq >> outputPath >> reportFilePath)) {
    printf("Function usage: DTPFC <tp_input_file> <frequency> <output_file> "
           "<report_file>\n");
    return 0;
  }

  // Verify input test pattern file exists
  std::ifstream tpCheck(tpInputFile);
  if (!tpCheck.is_open()) {
    printf("DTPFC: Unable to open test pattern input file: %s\n",
           tpInputFile.c_str());
    return 0;
  }
  tpCheck.close();

  const std::string resultsDir = "../dtpfc_results";
  const std::string resultsDirWithSlash = resultsDir + "/";

  auto ensureCleanDir = [](const std::string &dir) -> bool {
    if (dir.empty()) {
      return false;
    }
    struct stat sb{};
    if (stat(dir.c_str(), &sb) != 0) {
      if (mkdir(dir.c_str(), 0755) != 0) {
        printf("DTPFC: Unable to create results directory: %s\n", dir.c_str());
        return false;
      }
    } else if (!S_ISDIR(sb.st_mode)) {
      printf("DTPFC: Results path is not a directory: %s\n", dir.c_str());
      return false;
    }

    DIR *dirStream = opendir(dir.c_str());
    if (!dirStream) {
      printf("DTPFC: Unable to open results directory: %s\n", dir.c_str());
      return false;
    }

    struct dirent *entry = nullptr;
    std::string entryPath;
    while ((entry = readdir(dirStream)) != nullptr) {
      if (std::strcmp(entry->d_name, ".") == 0 ||
          std::strcmp(entry->d_name, "..") == 0) {
        continue;
      }
      entryPath = dir + "/" + entry->d_name;
      struct stat entryStat{};
      if (stat(entryPath.c_str(), &entryStat) != 0) {
        continue;
      }
      if (S_ISREG(entryStat.st_mode)) {
        std::remove(entryPath.c_str());
      } else if (S_ISDIR(entryStat.st_mode)) {
        // Skip nested directories; users should manage them manually.
        continue;
      } else {
        std::remove(entryPath.c_str());
      }
    }

    closedir(dirStream);
    return true;
  };

  if (!ensureCleanDir(resultsDir)) {
    return 0;
  }

  auto basename = [](const std::string &path) -> std::string {
    auto pos = path.find_last_of("/\\");
    if (pos == std::string::npos) {
      return path;
    }
    return path.substr(pos + 1);
  };

  outputPath = basename(outputPath);
  reportFilePath = basename(reportFilePath);

  const std::string rflFile = resultsDirWithSlash + "tpfc_rfl.txt";
  const std::string dfsFile = resultsDirWithSlash + "tpfc_dfs_fl_out.txt";
  const std::string reportFullPath = resultsDirWithSlash + reportFilePath;
  const std::string outputFullPath = resultsDirWithSlash + outputPath;

  // DTPFC: Use input file directly instead of RTPG
  // (This is the key difference from TPFC which uses RTPG)
  std::string rflArgs = rflFile;
  std::string dfsArgs = tpInputFile + " " + dfsFile;

  // No RTPG call - use the provided test pattern file directly
  simulator.setCommandArgs(rflArgs);
  simulator.reducedFaultList();
  simulator.setCommandArgs(dfsArgs);
  simulator.deductiveFaultSim(freq);

  const std::string rflPath = rflFile;
  auto loadFaultSet = [](const std::string &filePath) {
    std::unordered_set<std::string> faults;
    std::ifstream in(filePath.c_str());
    if (!in.is_open()) {
      return faults;
    }
    std::string line;
    while (std::getline(in, line)) {
      std::string trimmed = trimCopy(line);
      if (!trimmed.empty()) {
        faults.emplace(std::move(trimmed));
      }
    }
    return faults;
  };

  auto faultSet = loadFaultSet(rflPath);
  const std::size_t totalFaults = faultSet.size();
  if (totalFaults == 0) {
    printf("DTPFC: Reduced fault list is empty or missing, skipping coverage "
           "report generation.\n");
    return 0;
  }

  const std::string stepPrefix = "tpfc_dfs_fl_out_step_";
  const std::string stepSuffix = ".txt";
  std::vector<std::pair<int, std::string>> stepFiles;
  DIR *dir = opendir(resultsDir.c_str());
  if (!dir) {
    printf(
        "TPFC: Unable to open results directory '%s' to locate step files.\n",
        resultsDir.c_str());
    return 0;
  }
  struct dirent *entry = nullptr;
  while ((entry = readdir(dir)) != nullptr) {
    std::string filename = entry->d_name;
    if (filename.compare(0, stepPrefix.size(), stepPrefix) != 0 ||
        filename.size() <= stepPrefix.size() + stepSuffix.size() ||
        filename.substr(filename.size() - stepSuffix.size()) != stepSuffix) {
      continue;
    }
    struct stat sb;
    std::string fullPath = resultsDirWithSlash + filename;
    if (stat(fullPath.c_str(), &sb) != 0 || !S_ISREG(sb.st_mode)) {
      continue;
    }
    const std::string stepToken =
        filename.substr(stepPrefix.size(), filename.size() - stepPrefix.size() -
                                               stepSuffix.size());
    try {
      const int stepNum = std::stoi(stepToken);
      stepFiles.emplace_back(stepNum, fullPath);
    } catch (const std::exception &) {
      continue;
    }
  }
  closedir(dir);

  if (stepFiles.empty()) {
    printf("DTPFC: No DFS step files found, skipping coverage report "
           "generation.\n");
    return 0;
  }

  std::sort(stepFiles.begin(), stepFiles.end(),
            [](const std::pair<int, std::string> &lhs,
               const std::pair<int, std::string> &rhs) {
              return lhs.first < rhs.first;
            });

  std::ofstream reportFile(reportFullPath.c_str());
  if (!reportFile.is_open()) {
    printf("DTPFC: Unable to open report file: %s\n", reportFullPath.c_str());
    return 0;
  }
  reportFile << std::fixed << std::setprecision(6);
  std::unordered_set<std::string> detectedFaults;
  detectedFaults.reserve(totalFaults);
  std::string line;

  // Read step files for incremental coverage reporting
  for (std::size_t idx = 0; idx < stepFiles.size(); ++idx) {
    const int stepNum = stepFiles[idx].first;
    const std::string &path = stepFiles[idx].second;
    std::ifstream stepFile(path.c_str());
    if (!stepFile.is_open()) {
      printf("DTPFC: Unable to open step file: %s\n", path.c_str());
      reportFile << stepNum << ' '
                 << static_cast<double>(detectedFaults.size()) /
                        static_cast<double>(totalFaults)
                 << '\n';
      continue;
    }
    while (std::getline(stepFile, line)) {
      std::string trimmed = trimCopy(line);
      if (trimmed.empty()) {
        continue;
      }
      if (faultSet.find(trimmed) == faultSet.end()) {
        continue;
      }
      detectedFaults.insert(trimmed);
    }
    const double coverage = static_cast<double>(detectedFaults.size()) /
                            static_cast<double>(totalFaults);
    reportFile << stepNum << ' ' << coverage << '\n';
  }

  // Read FINAL DFS output file to get ALL detected faults
  // (step files might miss the last patterns if totalPatterns % freq != 0)
  std::ifstream finalDfsFile(dfsFile.c_str());
  if (finalDfsFile.is_open()) {
    while (std::getline(finalDfsFile, line)) {
      std::string trimmed = trimCopy(line);
      if (trimmed.empty()) {
        continue;
      }
      if (faultSet.find(trimmed) == faultSet.end()) {
        continue;
      }
      detectedFaults.insert(trimmed);
    }
    finalDfsFile.close();
  }

  // Calculate final coverage and elapsed time
  const double finalCoverage = static_cast<double>(detectedFaults.size()) /
                               static_cast<double>(totalFaults);
  auto endTime = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      endTime - startTime);
  double elapsedSeconds = duration.count() / 1000.0;

  // Write summary to report
  reportFile << "\n# Summary\n";
  reportFile << "# Total faults: " << totalFaults << '\n';
  reportFile << "# Detected faults: " << detectedFaults.size() << '\n';
  reportFile << "# Final coverage: " << finalCoverage << '\n';
  reportFile << "# Time elapsed: " << std::fixed << std::setprecision(3)
             << elapsedSeconds << " seconds\n";

  reportFile.close();

  // Print summary to console
  printf("DTPFC: Final coverage: %.6f (%zu/%zu faults)\n", finalCoverage,
         detectedFaults.size(), totalFaults);
  printf("DTPFC: Time elapsed: %.3f seconds\n", elapsedSeconds);

  std::ifstream source(dfsFile.c_str(), std::ios::binary);
  if (source.is_open()) {
    std::ofstream destination(outputFullPath.c_str(),
                              std::ios::binary | std::ios::trunc);
    if (!destination.is_open()) {
      printf("DTPFC: Failed to open final output file '%s' for writing.\n",
             outputFullPath.c_str());
    } else {
      destination << source.rdbuf();
    }
  } else {
    printf("DTPFC: DFS output file '%s' not found; coverage report only.\n",
           dfsFile.c_str());
  }
  return 1;
}
} // namespace logicsim
