/* vim:set ts=4 sw=4 sts=4 et: */

#include <algorithm>
#include <block/optimization.h>
#include <igraph/cpp/graph.h>

using namespace igraph;

namespace {
    double log_1_minus_x(double x) {
        return 1.0 - log(x);
    }
}

bool GreedyStrategy::step(UndirectedBlockmodel& model) {
    Graph* graph = model.getGraph();
    long int i, n = graph->vcount();
    int k = model.getNumTypes();
    Vector newTypes(n);

    // Calculate logP and log(1-P)
    Matrix logP = model.getProbabilities();
    Matrix log1P(logP);
    std::transform(logP.begin(), logP.end(), log1P.begin(), log_1_minus_x);
    std::transform(logP.begin(), logP.end(), logP.begin(), log);
    
    // Calculate logP - log(1-P), we will only need this, see explanation
    // below in the main loop
    logP -= log1P;

    // For each vertex...
    for (i = 0; i < n; i++) {
        // Approximate the local contribution of the vertex to the likelihood
        // when it is moved to each of the groups
        //
        // With some math magic, it can be shown that if the group of vertex i
        // is t_i, then the log-likelihood corresponding to the vertex is equal
        // to n * (log p(t_i, :) - log(1-p(t_i, :))) + const in Matlab notation.
        // n is a vector containing the number of neighbors i having type x and
        // the constant does not depend on n.
        //
        // So, first we calculate n, which we will denote with neiCountByType.
        Vector neiCountByType(k);
        Vector neis = graph->neighbors(i);
        for (Vector::iterator it = neis.begin(); it != neis.end(); it++) {
            neiCountByType[model.getType(*it)]++;
        }

        // We already have logP - log1P, so all we need is a matrix-vector
        // product
        neiCountByType = logP * neiCountByType;

        newTypes[i] = 
            std::max_element(neiCountByType.begin(), neiCountByType.end()) -
            neiCountByType.begin();
    }

    // TODO: rewrite the above to use a single matrix-matrix multiplication,
    // maybe that's faster?
    if (newTypes != model.getTypes()) {
        model.setTypes(newTypes);
        return true;
    }

    return false;
}

void GreedyStrategy::optimize(UndirectedBlockmodel& model) {
    while (step(model));
}

