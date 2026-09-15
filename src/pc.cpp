#include "../includes/core/pc.hpp"
#include "../includes/Simulator.hpp"
#include "../includes/utils.hpp"
#include <cstdio>

namespace logicsim {
    int pc_impl (Simulator &simulator) {
        int i, j;
        Node *np;

        printf(" Node   Type \tIn     \t\t\tOut    \n");
        printf("------ ------\t-------\t\t\t-------\n");
        for (i = 0; i < simulator.getNumNodes(); i++)
        {
            np = &simulator.getNodes()[i];
            printf("\t\t\t\t\t");
            for (j = 0; j < np->getFout(); j++)
                printf("%d ", np->getDnodes()[j]->getNum());
            printf("\r%5d  %s\t", np->getNum(), gname(np->getType()).c_str());
            for (j = 0; j < np->getFin(); j++)
                printf("%d ", np->getUnodes()[j]->getNum());
            printf("\n");
        }
        printf("Primary inputs:  ");
        for (i = 0; i < simulator.getNPI(); i++)
            printf("%d ", simulator.getPrimaryInputs()[i]->getNum());
        printf("\n");
        printf("Primary outputs: ");
        for (i = 0; i < simulator.getNPO(); i++)
            printf("%d ", simulator.getPrimaryOutputs()[i]->getNum());
        printf("\n\n");
        printf("Number of nodes = %d\n", simulator.getNumNodes());
        printf("Number of primary inputs = %d\n", simulator.getNPI());
        printf("Number of primary outputs = %d\n", simulator.getNPO());
        return 1;
    }
}
