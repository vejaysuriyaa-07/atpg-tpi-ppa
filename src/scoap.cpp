#include "../includes/core/scoap.hpp"
#include "../includes/Node.hpp"
#include "../includes/Simulator.hpp"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <vector>

namespace logicsim {

int syntacticComplexityOrientedAccessibilityAnalysis_impl(
    Simulator &simulator, const std::string &outFileName, bool atpg) {

  // Compute levels for the circuit
  auto &nodes = simulator.getNodes();
  for (auto &node : nodes) {
    node.setLevel(0);
  }
  const int INF = 999999;

  // Levelize the circuit so every node has a level value
  simulator.levelize();

  // Allocate working arrays indexed by node index
  int N = static_cast<int>(nodes.size());
  std::vector<int> cc0(N, INF), cc1(N, INF), co(N, INF);

  // Initialize controllability for PIs
  for (int i = 0; i < N; ++i) {
    if (nodes[i].getFin() == 0) {
      cc0[i] = 1;
      cc1[i] = 1;
    }
  }

  // Find max level
  int maxLevel = 0;
  for (int i = 0; i < N; ++i)
    if (nodes[i].getLevel() > maxLevel)
      maxLevel = nodes[i].getLevel();

  // Forward pass: compute CC0 and CC1
  for (int lvl = 0; lvl <= maxLevel; ++lvl) {
    for (int i = 0; i < N; ++i) {
      if (nodes[i].getLevel() != lvl)
        continue;
      if (nodes[i].getFin() == 0)
        continue; // PI

      if (nodes[i].getType() == BRCH) {
        Node **un = nodes[i].getUnodes();
        if (un && un[0]) {
          int up = un[0]->getIndx();
          cc0[i] = cc0[up];
          cc1[i] = cc1[up];
        }
        continue;
      }

      int fin = static_cast<int>(nodes[i].getFin());
      int sum_cc0 = 0, sum_cc1 = 0;
      int min_cc0 = INF, min_cc1 = INF;
      Node **un = nodes[i].getUnodes();
      for (int k = 0; k < fin; ++k) {
        int idx = un[k]->getIndx();
        sum_cc0 += cc0[idx];
        sum_cc1 += cc1[idx];
        if (cc0[idx] < min_cc0)
          min_cc0 = cc0[idx];
        if (cc1[idx] < min_cc1)
          min_cc1 = cc1[idx];
      }

      switch (nodes[i].getType()) {
      case XOR:
      case XNOR:
        if (fin == 2) {
          int a = un[0]->getIndx();
          int b = un[1]->getIndx();
          if (nodes[i].getType() == XOR) {
            cc0[i] = std::min(cc0[a] + cc0[b], cc1[a] + cc1[b]) + 1;
            cc1[i] = std::min(cc0[a] + cc1[b], cc1[a] + cc0[b]) + 1;
          } else { // XNOR
            cc0[i] = std::min(cc0[a] + cc1[b], cc1[a] + cc0[b]) + 1;
            cc1[i] = std::min(cc0[a] + cc0[b], cc1[a] + cc1[b]) + 1;
          }
        } else {
          cc0[i] = sum_cc0 + 1;
          cc1[i] = sum_cc1 + 1;
        }
        break;
      case OR:
        cc0[i] = sum_cc0 + 1;
        cc1[i] = min_cc1 + 1;
        break;
      case NOR:
        cc0[i] = min_cc1 + 1;
        cc1[i] = sum_cc0 + 1;
        break;
      case NOT:
        if (fin == 1) {
          int up = un[0]->getIndx();
          cc0[i] = cc1[up] + 1;
          cc1[i] = cc0[up] + 1;
        }
        break;
      case NAND:
        cc0[i] = sum_cc1 + 1;
        cc1[i] = min_cc0 + 1;
        break;
      case AND:
        cc0[i] = min_cc0 + 1;
        cc1[i] = sum_cc1 + 1;
        break;
      case BUF:
        if (fin > 0) {
          int up = un[0]->getIndx();
          cc0[i] = cc0[up];
          cc1[i] = cc1[up];
        }
        break;
      default:
        if (fin > 0) {
          int up = un[0]->getIndx();
          cc0[i] = cc0[up];
          cc1[i] = cc1[up];
        }
        break;
      }
    }
  }

  // Backward pass: compute CO
  for (int i = 0; i < N; ++i)
    co[i] = INF;
  for (int i = 0; i < N; ++i)
    if (nodes[i].getFout() == 0)
      co[i] = 0;

  bool updated = true;
  while (updated) {
    updated = false;
    for (int lvl = maxLevel; lvl >= 0; --lvl) {
      for (int i = 0; i < N; ++i) {
        if (nodes[i].getLevel() != lvl)
          continue;
        if (nodes[i].getType() == BRCH) {
          Node **un = nodes[i].getUnodes();
          if (un && un[0]) {
            int up = un[0]->getIndx();
            if (co[i] < co[up]) {
              co[up] = co[i];
              updated = true;
            }
          }
          continue;
        }

        if (co[i] == INF)
          continue;
        int fin = static_cast<int>(nodes[i].getFin());
        Node **un = nodes[i].getUnodes();
        for (int j = 0; j < fin; ++j) {
          Node *inN = un[j];
          int otherCost = 0;
          for (int k = 0; k < fin; ++k) {
            if (k == j)
              continue;
            Node *other = un[k];
            switch (nodes[i].getType()) {
            case AND:
            case NAND:
              otherCost += cc1[other->getIndx()];
              break;
            case OR:
            case NOR:
              otherCost += cc0[other->getIndx()];
              break;
            case XOR:
            case XNOR:
              otherCost +=
                  std::min(cc0[other->getIndx()], cc1[other->getIndx()]);
              break;
            default:
              break;
            }
          }
          int newCO = co[i] + otherCost + 1;
          if (newCO < co[inN->getIndx()]) {
            co[inN->getIndx()] = newCO;
            updated = true;
          }
        }
      }
    }
  }
  // Store SCOAP values in each node
  for (int i = 0; i < N; ++i) {
    nodes[i].setCC0(cc0[i]);
    nodes[i].setCC1(cc1[i]);
    nodes[i].setCO(co[i]);
  }

  if (!atpg) {
    // Output sorted by node number
    std::vector<int> indices(N);
    for (int i = 0; i < N; ++i)
      indices[i] = i;
    std::sort(indices.begin(), indices.end(), [&](int a, int b) {
      return nodes[a].getNum() < nodes[b].getNum();
    });

    // Ensure directory exists
    size_t last_slash = outFileName.find_last_of("/\\");
    if (last_slash != std::string::npos) {
      std::string dir = outFileName.substr(0, last_slash);
      std::string cmd = "mkdir -p '" + dir + "'";
      system(cmd.c_str());
    }

    FILE *fout = fopen(outFileName.c_str(), "w");
    if (!fout) {
      std::printf("Error opening output file: %s\n", outFileName.c_str());
      return 0;
    }

    for (int idx : indices) {
      fprintf(fout, "%d,%d,%d,%d\n", nodes[idx].getNum(), nodes[idx].getCC0(),
              nodes[idx].getCC1(), nodes[idx].getCO());
    }
    fclose(fout);
    std::printf("SCOAP analysis written to: %s\n", outFileName.c_str());
    return 1;
  } else {
    return 1;
  }
}
} // namespace logicsim
