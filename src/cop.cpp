#include "../includes/core/cop.hpp"
#include "../includes/Node.hpp"
#include "../includes/Simulator.hpp"
#include <algorithm>
#include <vector>

namespace logicsim {

CopResult computeCop(Simulator &simulator) {
  auto &nodes = simulator.getNodes();
  const std::size_t N = nodes.size();

  CopResult r;
  r.one.assign(N, 0.5);
  r.observe.assign(N, 0.0);

  int maxLevel = 0;
  for (auto &n : nodes)
    maxLevel = std::max(maxLevel, n.getLevel());

  // Bucket by level so both passes visit nodes in dependency order.
  std::vector<std::vector<std::size_t>> byLevel(maxLevel + 1);
  for (std::size_t i = 0; i < N; ++i)
    byLevel[nodes[i].getLevel()].push_back(i);

  // Forward: probability each line sits at 1. Inputs are unbiased, which is
  // what an LFSR gives you.
  for (int lvl = 0; lvl <= maxLevel; ++lvl) {
    for (std::size_t i : byLevel[lvl]) {
      Node &n = nodes[i];
      const unsigned fin = n.getFin();
      if (fin == 0) {
        r.one[i] = 0.5;
        continue;
      }
      Node **un = n.getUnodes();
      if (!un)
        continue;

      double prodOne = 1.0, prodZero = 1.0;
      for (unsigned k = 0; k < fin; ++k) {
        const double p = r.one[un[k]->getIndx()];
        prodOne *= p;
        prodZero *= (1.0 - p);
      }

      switch (n.getType()) {
      case AND:
        r.one[i] = prodOne;
        break;
      case NAND:
        r.one[i] = 1.0 - prodOne;
        break;
      case OR:
        r.one[i] = 1.0 - prodZero;
        break;
      case NOR:
        r.one[i] = prodZero;
        break;
      case NOT:
        r.one[i] = 1.0 - r.one[un[0]->getIndx()];
        break;
      case XOR:
      case XNOR: {
        // Odds of an odd number of ones, folded one input at a time.
        double odd = 0.0;
        for (unsigned k = 0; k < fin; ++k) {
          const double p = r.one[un[k]->getIndx()];
          odd = odd * (1.0 - p) + (1.0 - odd) * p;
        }
        r.one[i] = (n.getType() == XOR) ? odd : 1.0 - odd;
        break;
      }
      case BRCH:
      case BUF:
      default:
        r.one[i] = r.one[un[0]->getIndx()];
        break;
      }
    }
  }

  // Backward: odds a flip on a line survives to an output. Outputs see
  // themselves; everything else inherits the best of its fanouts.
  for (std::size_t i = 0; i < N; ++i)
    if (nodes[i].getFout() == 0)
      r.observe[i] = 1.0;

  for (int lvl = maxLevel; lvl >= 0; --lvl) {
    for (std::size_t i : byLevel[lvl]) {
      Node &n = nodes[i];
      const unsigned fin = n.getFin();
      if (fin == 0 || r.observe[i] <= 0.0)
        continue;
      Node **un = n.getUnodes();
      if (!un)
        continue;

      for (unsigned j = 0; j < fin; ++j) {
        // The other inputs have to be holding their non-controlling value,
        // otherwise they mask the flip before it moves.
        double pass = 1.0;
        for (unsigned k = 0; k < fin; ++k) {
          if (k == j)
            continue;
          const double p = r.one[un[k]->getIndx()];
          switch (n.getType()) {
          case AND:
          case NAND:
            pass *= p;
            break;
          case OR:
          case NOR:
            pass *= (1.0 - p);
            break;
          default:
            break; // XOR/XNOR and single-input cells never mask
          }
        }
        const std::size_t up = un[j]->getIndx();
        // A stem keeps the best of its branches rather than summing them,
        // which would double count the same pattern detecting it twice.
        r.observe[up] = std::max(r.observe[up], r.observe[i] * pass);
      }
    }
  }

  return r;
}

} // namespace logicsim
