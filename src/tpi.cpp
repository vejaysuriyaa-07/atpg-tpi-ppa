#include "../includes/core/tpi.hpp"
#include "../includes/Node.hpp"
#include "../includes/Simulator.hpp"
#include "../includes/core/cop.hpp"
#include "../includes/core/ppa.hpp"
#include "../includes/core/scoap.hpp"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace logicsim {
namespace {

const int SCOAP_INF = 999999;

// An editable copy of the netlist. The Node class hangs on to raw fanin and
// fanout pointer arrays that were sized at parse time, so growing the circuit
// in place means reallocating all of it. Far easier to pull the structure out,
// rewrite it here, and let cread build the real thing again from the file.
struct EditNode {
  int id = 0;
  NodeType ntype = GATE;
  GateType gtype = IPT;
  std::vector<int> inputs;
};

struct EditNetlist {
  std::vector<EditNode> nodes;
  std::unordered_map<int, int> byId;

  EditNode *find(int id) {
    auto it = byId.find(id);
    return it == byId.end() ? nullptr : &nodes[it->second];
  }

  void reindex() {
    byId.clear();
    for (std::size_t i = 0; i < nodes.size(); ++i)
      byId[nodes[i].id] = static_cast<int>(i);
  }

  int add(EditNode n) {
    byId[n.id] = static_cast<int>(nodes.size());
    nodes.push_back(std::move(n));
    return nodes.back().id;
  }

  // Fanout is never stored -- it is whatever references the node, which keeps
  // it correct after the structure has been rewired.
  std::unordered_map<int, std::vector<int>> fanoutMap() const {
    std::unordered_map<int, std::vector<int>> fo;
    for (const auto &n : nodes)
      for (int in : n.inputs)
        fo[in].push_back(n.id);
    return fo;
  }
};

EditNetlist snapshot(Simulator &sim) {
  EditNetlist nl;
  auto &nodes = sim.getNodes();
  nl.nodes.reserve(nodes.size());
  for (auto &node : nodes) {
    EditNode e;
    e.id = static_cast<int>(node.getNum());
    e.ntype = node.getNtype();
    e.gtype = node.getType();
    Node **un = node.getUnodes();
    for (unsigned i = 0; i < node.getFin(); ++i)
      if (un && un[i])
        e.inputs.push_back(static_cast<int>(un[i]->getNum()));
    nl.nodes.push_back(std::move(e));
  }
  nl.reindex();
  return nl;
}

bool writeNetlist(const EditNetlist &nl, const std::string &path) {
  auto fo = nl.fanoutMap();
  std::ofstream out(path);
  if (!out.is_open())
    return false;

  for (const auto &n : nl.nodes) {
    auto it = fo.find(n.id);
    const int fanout = (it == fo.end()) ? 0 : static_cast<int>(it->second.size());

    if (n.ntype == FB) {
      // Branches carry no counts in this format, just the stem they hang off.
      out << FB << " " << n.id << " " << BRCH << " " << n.inputs[0] << "\n";
      continue;
    }

    out << static_cast<int>(n.ntype) << " " << n.id << " "
        << static_cast<int>(n.gtype) << " " << fanout << " " << n.inputs.size();
    for (int in : n.inputs)
      out << " " << in;
    out << "\n";
  }
  return true;
}

// Longest path through the circuit under the cell delay model. Used for the
// timing half of the PPA estimate.
double criticalPath(const EditNetlist &nl, const PpaModel &model) {
  std::unordered_map<int, int> indeg;
  auto fo = nl.fanoutMap();
  for (const auto &n : nl.nodes)
    indeg[n.id] = static_cast<int>(n.inputs.size());

  std::unordered_map<int, double> arrival;
  std::vector<int> ready;
  for (const auto &n : nl.nodes)
    if (n.inputs.empty()) {
      arrival[n.id] = 0.0;
      ready.push_back(n.id);
    }

  EditNetlist &mutable_nl = const_cast<EditNetlist &>(nl);
  double worst = 0.0;
  while (!ready.empty()) {
    int cur = ready.back();
    ready.pop_back();
    worst = std::max(worst, arrival[cur]);
    auto it = fo.find(cur);
    if (it == fo.end())
      continue;
    for (int nxt : it->second) {
      EditNode *node = mutable_nl.find(nxt);
      if (!node)
        continue;
      double d = arrival[cur] +
                 model.delayOf(node->gtype, static_cast<unsigned>(node->inputs.size()));
      arrival[nxt] = std::max(arrival[nxt], d);
      if (--indeg[nxt] == 0)
        ready.push_back(nxt);
    }
  }
  return worst;
}

double totalArea(const EditNetlist &nl, const PpaModel &model) {
  double a = 0.0;
  for (const auto &n : nl.nodes)
    a += model.areaOf(n.gtype, static_cast<unsigned>(n.inputs.size()));
  return a;
}

} // namespace

// Picks the nodes worth instrumenting, rewrites the netlist around them, and
// prices the result.
int testPointInsertion_impl(Simulator &simulator) {
  std::istringstream args(simulator.getCommandArgs());
  int budget = 0;
  std::string outNetlist, reportPath;
  if (!(args >> budget >> outNetlist >> reportPath)) {
    std::fprintf(stderr,
                 "Usage: TPI <budget> <out_netlist.ckt> <report.csv> "
                 "[-mode cp|op|mixed] [-metric cop|scoap]\n       [-weight <n>] [-bias <f>] [-ppa <model_file>]\n");
    return 0;
  }
  if (budget <= 0) {
    std::fprintf(stderr, "TPI: budget must be positive\n");
    return 0;
  }

  std::string mode = "mixed";
  std::string metric = "cop";
  int weight = 3;
  double bias = 4.0;
  std::string ppaFile;
  std::string flag;
  while (args >> flag) {
    if (flag == "-mode")
      args >> mode;
    else if (flag == "-metric")
      args >> metric;
    else if (flag == "-weight")
      args >> weight;
    else if (flag == "-bias")
      args >> bias;
    else if (flag == "-ppa")
      args >> ppaFile;
  }
  if (mode != "cp" && mode != "op" && mode != "mixed") {
    std::fprintf(stderr, "TPI: mode must be cp, op or mixed\n");
    return 0;
  }
  if (bias <= 0.0) {
    std::fprintf(stderr, "TPI: bias must be positive\n");
    return 0;
  }
  if (weight < 1 || weight > 8) {
    std::fprintf(stderr, "TPI: weight must be between 1 and 8\n");
    return 0;
  }
  if (metric != "cop" && metric != "scoap") {
    std::fprintf(stderr, "TPI: metric must be cop or scoap\n");
    return 0;
  }

  PpaModel model;
  if (!ppaFile.empty()) {
    std::string err;
    if (!model.loadFromFile(ppaFile, err)) {
      std::fprintf(stderr, "TPI: %s\n", err.c_str());
      return 0;
    }
  }

  // SCOAP drives the whole selection, so it has to run first. The atpg flag
  // keeps it from writing its own report file.
  syntacticComplexityOrientedAccessibilityAnalysis_impl(simulator, "", true);

  EditNetlist nl = snapshot(simulator);
  const double baseArea = totalArea(nl, model);
  const double baseDelay = criticalPath(nl, model);

  // Candidates are internal gate outputs. Primary outputs are already observed,
  // branches are just wires off a stem, and a point belongs on the stem.
  CopResult cop = computeCop(simulator);

  struct Candidate {
    int id;
    double detect;   // worst of the two stuck-at odds at this line
    double one;      // P(line = 1)
    double observe;  // P(a flip here reaches an output)
  };

  std::vector<Candidate> cands;
  auto &simNodes = simulator.getNodes();
  for (std::size_t i = 0; i < simNodes.size(); ++i) {
    Node &node = simNodes[i];
    if (node.getNtype() != GATE || node.getType() == BRCH)
      continue;
    if (node.getFout() == 0)
      continue;

    Candidate c;
    c.id = static_cast<int>(node.getNum());
    if (metric == "cop") {
      c.one = cop.one[i];
      c.observe = cop.observe[i];
      c.detect = std::min(cop.detect0(i), cop.detect1(i));
    } else {
      // SCOAP costs turned into something sortable on the same scale. Kept so
      // the two metrics can be run head to head -- see docs/metrics.md.
      const int cc0 = node.getCC0(), cc1 = node.getCC1(), co = node.getCO();
      if (cc0 >= SCOAP_INF || cc1 >= SCOAP_INF || co >= SCOAP_INF)
        continue;
      c.one = (cc1 > cc0) ? 0.25 : 0.75;
      c.observe = 1.0 / (1.0 + co);
      c.detect = c.observe / (1.0 + std::max(cc0, cc1));
    }
    cands.push_back(c);
  }

  TpiResult result;
  result.nodesConsidered = static_cast<int>(cands.size());
  if (cands.empty()) {
    std::fprintf(stderr, "TPI: no eligible nodes\n");
    return 0;
  }

  // Hardest to detect first. That ordering is the whole point: these are the
  // lines random patterns keep missing, and they are what the budget buys.
  //
  // Plenty of lines tie on detection probability -- symmetric logic gives whole
  // groups the same number -- and std::sort is not stable, so without the line
  // number as a second key the winners depend on the standard library. That is
  // how the same budget on the same circuit selected a different mix under
  // libc++ than under libstdc++.
  std::sort(cands.begin(), cands.end(),
            [](const Candidate &a, const Candidate &b) {
              if (a.detect != b.detect)
                return a.detect < b.detect;
              return a.id < b.id;
            });

  // Two points sitting next to each other mostly fix the same faults, so once
  // a node is taken its immediate neighbours are off the table.
  auto fanout = nl.fanoutMap();
  std::unordered_set<int> blocked;
  auto claim = [&](int id) {
    blocked.insert(id);
    EditNode *n = nl.find(id);
    if (n)
      for (int in : n->inputs)
        blocked.insert(in);
    auto it = fanout.find(id);
    if (it != fanout.end())
      for (int out : it->second)
        blocked.insert(out);
  };

  for (const auto &c : cands) {
    if (static_cast<int>(result.points.size()) >= budget)
      break;
    if (blocked.count(c.id))
      continue;

    // Which way the node is stuck decides the fix. If a flip cannot get out,
    // no amount of driving helps and it needs watching instead; otherwise the
    // value that is hard to reach is the one the new pin forces.
    // Whichever probability is smaller is the thing holding detection back.
    // The comparison is tilted though: an observe point exposes the node's
    // whole cone and cannot make anything worse, while a control point only
    // helps its own fanout and still blocks the real signal some of the time.
    // bias > 1 pays for that asymmetry; the default came out of the sweep in
    // docs/metrics.md.
    const double ctrl = std::min(c.one, 1.0 - c.one);
    const bool observeLimited = c.observe < ctrl * bias;

    TestPoint tp;
    tp.node = c.id;
    tp.score = c.detect;
    if (mode == "op" || (mode == "mixed" && observeLimited))
      tp.kind = TestPointKind::Observe;
    else
      tp.kind = (c.one < 0.5) ? TestPointKind::Control1 : TestPointKind::Control0;

    claim(tp.node);
    result.points.push_back(tp);
  }

  if (result.points.empty()) {
    std::fprintf(stderr, "TPI: nothing selected\n");
    return 0;
  }

  // Rewire. New lines are numbered above everything that already exists so no
  // renumbering of the original circuit is needed.
  int nextId = 0;
  for (const auto &n : nl.nodes)
    nextId = std::max(nextId, n.id);
  ++nextId;

  int addedGates = 0, addedPis = 0, addedPos = 0;
  double wirelength = 0.0;

  int maxLevel = 1;
  for (auto &node : simNodes)
    maxLevel = std::max(maxLevel, node.getLevel());

  std::unordered_map<int, int> levelOf;
  for (auto &node : simNodes)
    levelOf[static_cast<int>(node.getNum())] = node.getLevel();

  for (const auto &tp : result.points) {
    auto fo = nl.fanoutMap();
    auto sinksIt = fo.find(tp.node);
    std::vector<int> sinks =
        (sinksIt == fo.end()) ? std::vector<int>{} : sinksIt->second;

    if (tp.kind == TestPointKind::Observe) {
      // Tap the node out to a fresh primary output. If it only drove one sink
      // it was an unbranched stem, so it needs branch lines now that it drives
      // two things.
      if (sinks.size() == 1) {
        EditNode b1;
        b1.id = nextId++;
        b1.ntype = FB;
        b1.gtype = BRCH;
        b1.inputs = {tp.node};
        nl.add(b1);

        EditNode *sink = nl.find(sinks[0]);
        for (auto &in : sink->inputs)
          if (in == tp.node)
            in = b1.id;
      }

      EditNode b2;
      b2.id = nextId++;
      b2.ntype = FB;
      b2.gtype = BRCH;
      b2.inputs = {tp.node};
      nl.add(b2);

      EditNode po;
      po.id = nextId++;
      po.ntype = PO;
      po.gtype = BUF;
      po.inputs = {b2.id};
      nl.add(po);

      ++addedPos;
      ++addedGates;
      nl.reindex();
    } else {
      // Splice a gate between the node and everything it drove. AND forces 0,
      // OR forces 1.
      //
      // The enable cannot come straight off a bare pin. At even odds the point
      // blocks the functional signal half the time and loses more coverage
      // downstream than it buys at the node -- measured, see docs/metrics.md.
      // Gating it through a small tree drops the activation rate to 2^-weight
      // so normal propagation survives.
      std::vector<int> pins;
      for (int w = 0; w < weight; ++w) {
        EditNode pin;
        pin.id = nextId++;
        pin.ntype = PI;
        pin.gtype = IPT;
        nl.add(pin);
        pins.push_back(pin.id);
        ++addedPis;
      }

      int enable = pins[0];
      if (weight > 1) {
        EditNode tree;
        tree.id = nextId++;
        tree.ntype = GATE;
        // A control-0 point fires when its enable drops, so the tree has to
        // hold that enable high; control-1 is the mirror image.
        tree.gtype = (tp.kind == TestPointKind::Control1) ? AND : OR;
        tree.inputs = pins;
        nl.add(tree);
        enable = tree.id;
        ++addedGates;
      }

      EditNode g;
      g.id = nextId++;
      g.ntype = GATE;
      g.gtype = (tp.kind == TestPointKind::Control1) ? OR : AND;
      g.inputs = {tp.node, enable};
      nl.add(g);

      nl.reindex();
      for (int s : sinks) {
        EditNode *sink = nl.find(s);
        if (!sink)
          continue;
        for (auto &in : sink->inputs)
          if (in == tp.node)
            in = g.id;
      }

      ++addedGates;
      nl.reindex();
    }

    // Placement proxy: levelised depth stands in for how far the new net has
    // to run back to the circuit boundary. Innovus replaces this outright.
    const int lvl = levelOf.count(tp.node) ? levelOf[tp.node] : 0;
    wirelength += 1.0 + static_cast<double>(maxLevel - lvl) / maxLevel;
  }

  nl.reindex();

  if (!writeNetlist(nl, outNetlist)) {
    std::fprintf(stderr, "TPI: cannot write %s\n", outNetlist.c_str());
    return 0;
  }

  PpaEstimate &ppa = result.ppa;
  ppa.baseArea = baseArea;
  ppa.addedArea = totalArea(nl, model) - baseArea;
  ppa.areaPercent = baseArea > 0.0 ? 100.0 * ppa.addedArea / baseArea : 0.0;
  ppa.baseDelay = baseDelay;
  ppa.newDelay = criticalPath(nl, model);
  ppa.delayPercent =
      baseDelay > 0.0 ? 100.0 * (ppa.newDelay - baseDelay) / baseDelay : 0.0;
  ppa.wirelengthProxy = wirelength;
  ppa.addedGates = addedGates;
  ppa.addedPis = addedPis;
  ppa.addedPos = addedPos;
  ppa.source = model.source;

  std::ofstream rep(reportPath);
  if (!rep.is_open()) {
    std::fprintf(stderr, "TPI: cannot write %s\n", reportPath.c_str());
    return 0;
  }
  rep << "circuit,mode,metric,budget,test_points,control_points,observe_points,"
         "added_gates,added_pis,added_pos,base_area_um2,added_area_um2,"
         "area_pct,base_delay_ns,new_delay_ns,delay_pct,wirelength_proxy,"
         "ppa_source\n";
  int nCtl = 0, nObs = 0;
  for (const auto &tp : result.points)
    (tp.kind == TestPointKind::Observe) ? ++nObs : ++nCtl;
  rep << simulator.getInpName() << "," << mode << "," << metric << "," << budget << ","
      << result.points.size() << "," << nCtl << "," << nObs << "," << addedGates
      << "," << addedPis << "," << addedPos << "," << ppa.baseArea << ","
      << ppa.addedArea << "," << ppa.areaPercent << "," << ppa.baseDelay << ","
      << ppa.newDelay << "," << ppa.delayPercent << "," << ppa.wirelengthProxy
      << "," << ppa.source << "\n";
  rep.close();

  const std::string detailPath = reportPath + ".points";
  std::ofstream det(detailPath);
  if (det.is_open()) {
    det << "node,kind,score\n";
    for (const auto &tp : result.points) {
      const char *k = tp.kind == TestPointKind::Observe ? "observe"
                      : tp.kind == TestPointKind::Control1 ? "control1"
                                                           : "control0";
      det << tp.node << "," << k << "," << tp.score << "\n";
    }
  }

  std::printf("Test points inserted: %zu (%d control, %d observe) from %d candidates\n",
              result.points.size(), nCtl, nObs, result.nodesConsidered);
  std::printf("Area: %.2f -> %.2f um2 (+%.2f%%)\n", ppa.baseArea,
              ppa.baseArea + ppa.addedArea, ppa.areaPercent);
  std::printf("Critical path: %.3f -> %.3f ns (%+.2f%%)\n", ppa.baseDelay,
              ppa.newDelay, ppa.delayPercent);
  std::printf("PPA source: %s\n", ppa.source.c_str());
  std::printf("Netlist written to: %s\n", outNetlist.c_str());
  std::printf("Report written to: %s\n", reportPath.c_str());
  return 1;
}

} // namespace logicsim
