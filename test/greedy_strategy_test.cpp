/* vim:set ts=4 sw=4 sts=4 et: */

#include <cstdlib>
#include <igraph/cpp/edge_selector.h>
#include <igraph/cpp/graph.h>
#include <igraph/cpp/generators/full.h>
#include <igraph/cpp/generators/grg.h>
#include <igraph/cpp/generators/ring.h>
#include <block/blockmodel.h>
#include <block/optimization.hpp>

#include "test_common.cpp"

using namespace igraph;

int test_two_rings() {
    Graph graph = *ring(5) + *ring(5);
    Vector types(10);
    GreedyStrategy<UndirectedBlockmodel> greedy;
    UndirectedBlockmodel model;
    model.setGraph(&graph);
    model.setNumTypes(2);

    /* First, set up the optimal configuration and see if we stay there */
    for (int i = 0; i < 10; i++)
        types[i] = i/5;
    model.setTypes(types);

    greedy.optimize(&model);
    if (model.getTypes() != types)
        return 1;

    /* Change one element and see if we converge back */
    model.setType(0, 1);
    greedy.optimize(&model);
    if (model.getTypes() != types)
        return 2;

    return 0;
}

int test_four_almost_cliques() {
    Graph graph = *full(4) + *full(4) + *full(4) + *full(4);
    Vector types(16);
    GreedyStrategy<UndirectedBlockmodel> greedy;
    UndirectedBlockmodel model;

    model.setGraph(&graph);
    model.setNumTypes(4);

    /* Remove one edge from each clique */
    Vector edges(8);
    edges[0] =  0; edges[1] =  1; edges[2] =  5; edges[3] =  6;
    edges[4] = 10; edges[5] = 11; edges[6] = 15; edges[7] = 12;
    graph.deleteEdges(EdgeSelector::Pairs(edges));

    /* Every clique is colored differently */
    for (int i = 0; i < 16; i++)
        types[i] = i / 4;
    model.setTypes(types);

    /* Check if we stay in this configuration */
    greedy.optimize(&model);
    if (model.getTypes() != types)
        return 1;

    /* Mutate one color in each clique */
    for (int i = 0; i < 4; i++)
        model.setType(i*4 + rand() % 4, rand() % 4);
    model.setTypes(types);

    /* Check if we stay in this configuration */
    greedy.optimize(&model);
    if (model.getTypes() != types) {
        model.getTypes().print();
        return 2;
    }

    return 0;
}

/* This test is skipped currently as the greedy strategy uses an *approximation*
 * only, which prevents it from finding an exact match */
int test_grg() {
    Graph graph = *grg_game(100, 0.2);
    GreedyStrategy<UndirectedBlockmodel> greedy;
    MersenneTwister rng;
    Vector expected(graph.vcount());
    UndirectedBlockmodel model;

    model.setGraph(&graph);
    model.setNumTypes(4);

    model.randomize(rng);

    // Fill the expected types vector with the expected result
    for (int i = 0; i < graph.vcount(); i++) {
        int oldType = model.getType(i);
        double bestLogL = model.getLogLikelihood(), logL;

        expected[i] = -1;
        for (int j = 0; j < model.getNumTypes(); j++) {
            model.setType(i, j);
            logL = model.getLogLikelihood();
            if (logL > bestLogL) {
                bestLogL = logL;
                expected[i] = j;
            } else if (logL == bestLogL) {
                expected[i] = -1;  /* ambiguous */
            }
        }

        model.setType(i, oldType);
    }

    greedy.step(&model);

    for (size_t i = 0; i < expected.size(); i++)
        if (expected[i] == -1)
            expected[i] = model.getType(i);

    if (model.getTypes() != expected) {
        printf("Expected: ");
        expected.print();
        printf("Observed: ");
        model.getTypes().print();
        return 1;
    }

    return 0;
}

int main(int argc, char* argv[]) {
    srand(time(0));

    CHECK(test_two_rings);
    CHECK(test_four_almost_cliques);
    // CHECK(test_grg);

    return 0;
}

