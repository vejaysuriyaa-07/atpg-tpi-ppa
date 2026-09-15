#include "../includes/core/lev.hpp"
#include "../includes/Simulator.hpp"
#include "../includes/utils.hpp"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <string>

namespace logicsim {
    int lev_impl(Simulator &simulator, bool writeReport) {
        int i, j, Ngates = 0, none_flag = 0;
        for (i = 0; i < simulator.getNumNodes(); i++)
        {
            simulator.getNodes()[i].setLevel(-1); // Initialize levels to none (-1)
            if (simulator.getNodes()[i].getType() >= 2)
            {
                Ngates++;
            }
            if (simulator.getNodes()[i].getType() == 0)
            {
                simulator.getNodes()[i].setLevel(0); // Primary inputs are at level 0
            }
        }
        i = 0;
        none_flag = 1;
        while (none_flag != 0)
        {
            none_flag = 0;
            // For all nodes:
            for (i = 0; i < simulator.getNumNodes(); i++)
            {
                // Check if any node is not assigned to a level
                if (simulator.getNodes()[i].getLevel() == -1)
                {
                    none_flag = 1;
                }
                // Iterate through fanin (up nodes)
                for (int k = 0; k < simulator.getNodes()[i].getFin(); k++)
                {
                    // Pass if its fanin node is not assigned to a level
                    if (simulator.getNodes()[i].getUnodes()[k]->getLevel() != -1)
                    {
                        // Update the level of the current node
                        if (simulator.getNodes()[i].getType() == 1)
                        {
                            simulator.getNodes()[i].setLevel(simulator.getNodes()[i].getUnodes()[k]->getLevel() + 1);
                        }
                        else
                        {
                            simulator.getNodes()[i].setLevel(std::max(simulator.getNodes()[i].getLevel(), simulator.getNodes()[i].getUnodes()[k]->getLevel() + 1));
                        }
                    }
                    else
                    {
                        simulator.getNodes()[i].setLevel(-1);
                        break;
                    }
                }
            }
        }
        if (!writeReport) {
            return 1;
        }
        size_t pos = simulator.getInpName().find_last_of("/\\");
        std::string filename = (pos == std::string::npos) ? simulator.getInpName() : simulator.getInpName().substr(pos + 1);
        size_t dot_pos = filename.find_last_of('.');
        filename = filename.substr(0, dot_pos);
        std::ofstream outfile;
        std::string outputPath = logicsim::trimCopy(simulator.getCommandArgs());
        if (outputPath.empty()) {
            printf("LEV requires an output file path.\n");
            return 0;
        }
        outfile.open(outputPath.c_str());
        outfile << filename.c_str() << "\n";
        outfile << "#PI: " << simulator.getNPI() << "\n";
        outfile << "#PO: " << simulator.getNPO() << "\n";
        outfile << "#Nodes: " << simulator.getNumNodes() << "\n";
        outfile << "#Gates: " << Ngates << "\n";
        for (i = 0; i < simulator.getNumNodes(); i++)
        {
            outfile << simulator.getNodes()[i].getNum() << " " << simulator.getNodes()[i].getLevel() << "\n";
        }
        outfile.close();
        return 1;
    }
}
