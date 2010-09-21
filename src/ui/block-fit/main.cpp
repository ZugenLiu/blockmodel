/* vim:set ts=4 sw=4 sts=4 et: */

#include <csignal>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <block/blockmodel.h>
#include <block/convergence.h>
#include <block/io.hpp>
#include <block/optimization.h>
#include <block/util.hpp>
#include <igraph/cpp/graph.h>

#include "../common/logging.h"
#include "cmd_arguments.h"

using namespace igraph;
using namespace std;

class BlockmodelFittingApp {
private:
    /// Parsed command line arguments
    CommandLineArguments m_args;

    /// Graph being analyzed by the UI
    std::auto_ptr<Graph> m_pGraph;

    /// Blockmodel being fitted to the graph
    std::auto_ptr<UndirectedBlockmodel> m_pModel;

    /// Markov chain Monte Carlo strategy to optimize the model
    MetropolisHastingsStrategy m_mcmc;

    /// Best log-likelihood found so far
    double m_bestLogL;

    /// Best model found so far
    UndirectedBlockmodel m_bestModel;

    /// Flag to note whether we have to dump the best state when possible
    bool m_dumpBestStateFlag;

    /// Writer object that is used to dump the best state
    std::auto_ptr<Writer<UndirectedBlockmodel> > m_pModelWriter;

public:
    LOGGING_FUNCTION(debug, 2);
    LOGGING_FUNCTION(info, 1);
    LOGGING_FUNCTION(error, 0);

    /// Constructor
    BlockmodelFittingApp() : m_pGraph(0), m_pModel(0),
        m_bestLogL(-std::numeric_limits<double>::max()),
        m_bestModel(), m_dumpBestStateFlag(false),
        m_pModelWriter(0) {}

    /// Dumps the best state found so far and clears the dump flag
    void dumpBestState() {
        info(">> dumping best state of the chain");
        if (m_pModelWriter.get()) {
            m_pModelWriter->write(m_bestModel, cout);
        } else {
            debug(">> no model writer set up, printing nothing");
        }
        m_dumpBestStateFlag = false;
    }

    /// Fits the blockmodel to the data using a given group count
    void fitForGivenGroupCount(int groupCount) {
        bool converged = false;
        Vector samples(m_args.blockSize);

        m_pModel.reset(new UndirectedBlockmodel(m_pGraph.get(), groupCount));
        m_pModel->randomize(*m_mcmc.getRNG());

        if (m_args.initMethod == GREEDY) {
            GreedyStrategy greedy;
            greedy.setModel(m_pModel.get());

            info(">> running greedy initialization");

            while (greedy.step()) {
                double logL = m_pModel->getLogLikelihood();
                if (!isQuiet()) {
                    clog << '[' << setw(6) << greedy.getStepCount() << "] "
                         << '(' << setw(2) << m_pModel->getNumTypes() << ") "
                         << setw(12) << logL << "\t(" << logL << ")\n";
                }
            }
        }

        m_bestModel = *m_pModel;
        m_bestLogL = m_pModel->getLogLikelihood();
        m_mcmc.setModel(m_pModel.get());

        info(">> starting Markov chain");

        // Run the Markov chain until convergence
		std::auto_ptr<ConvergenceCriterion> pConvCrit(new EntropyConvergenceCriterion());
        while (!converged) {
            runBlock(m_args.blockSize, samples);
			converged = pConvCrit->check(samples);

			std::string report = pConvCrit->report();
			if (report.size() > 0)
				debug(">> %s", report.c_str());
        }
    }

    /// Returns whether we are running in quiet mode
    bool isQuiet() {
        return m_args.verbosity < 1;
    }

    /// Returns whether we are running in verbose mode
    bool isVerbose() {
        return m_args.verbosity > 1;
    }

    /// Loads a graph from the given file
    /**
     * If the name of the file is "-", the file is assumed to be the
     * standard input.
     */
    std::auto_ptr<Graph> loadGraph(const std::string& filename) {
        std::auto_ptr<Graph> result;
        FILE* fptr = stdin;

        if (filename != "-") {
            fptr = fopen(filename.c_str(), "r");
            if (fptr == NULL) {
                std::ostringstream oss;
                oss << "File not found: " << filename;
                throw std::runtime_error(oss.str());
            }
        }

        result.reset(new Graph(Graph::ReadEdgelist(fptr)));
        if (filename != "-")
            result->setAttribute("filename", filename);

        return result;
    }

    /// Dumps the best state of the model to a file on the next occasion
    /**
     * This function is called by the SIGUSR1 signal handler to signal that
     * we should dump the best state as soon as possible. We cannot dump
     * it here directly as we might be modifying it at the same time.
     */
    void raiseDumpBestStateFlag() {
        m_dumpBestStateFlag = true;
    }

    /// Runs a single block of the Markov chain Monte Carlo process
    /**
     * The sampled log-likelihoods are collected in the given vector.
     * The vector is cleared at the start of the process.
     */
    void runBlock(long numSamples, Vector& samples) {
        double logL;

        samples.clear();
        while (numSamples > 0) {
            m_mcmc.step();

            logL = m_pModel->getLogLikelihood();
            if (m_bestLogL < logL) {
                // Store the best model and log-likelihood
                m_bestModel = *m_pModel.get();
                m_bestLogL = logL;
            }
            samples.push_back(logL);

            if (m_mcmc.getStepCount() % m_args.logPeriod == 0 && !isQuiet()) {
                clog << '[' << setw(6) << m_mcmc.getStepCount() << "] "
                     << '(' << setw(2) << m_pModel->getNumTypes() << ") "
                     << setw(12) << logL << "\t(" << m_bestLogL << ")\t"
                     << (m_mcmc.wasLastProposalAccepted() ? '*' : ' ')
                     << setw(8) << m_mcmc.getAcceptanceRatio()
                     << '\n';
            }

            if (m_dumpBestStateFlag)
                dumpBestState();

            numSamples--;
        }
    }

    /// Runs the sampling until hell freezes over
    void runUntilHellFreezesOver() {
        Vector dummy;

        while (1)
            runBlock(1000, dummy);
    }

    /// Runs the user interface
    int run(int argc, char** argv) {
        m_args.parse(argc, argv);

        switch (m_args.outputFormat) {
            case FORMAT_JSON:
                m_pModelWriter.reset(new JSONWriter<UndirectedBlockmodel>);
                break;

			case FORMAT_NULL:
                m_pModelWriter.reset(new NullWriter<UndirectedBlockmodel>);
                break;

            default:
                m_pModelWriter.reset(new PlainTextWriter<UndirectedBlockmodel>);
        };

        info(">> loading graph: %s", m_args.inputFile.c_str());
        m_pGraph = loadGraph(m_args.inputFile);
        info(">> graph has %ld vertices and %ld edges",
             (long)m_pGraph->vcount(), (long)m_pGraph->ecount());

        debug(">> using random seed: %lu", m_args.randomSeed);
        m_mcmc.getRNG()->init_genrand(m_args.randomSeed);

        if (m_args.numGroups > 0) {
            /* Run the Markov chain until it converges */
            fitForGivenGroupCount(m_args.numGroups);
            info(">> AIC = %.4f, BIC = %.4f", aic(*m_pModel), bic(*m_pModel));
        } else {
            double currentAIC, bestAIC = std::numeric_limits<double>::max();
            double currentBIC, bestBIC = std::numeric_limits<double>::max();
            UndirectedBlockmodel bestModel;

            /* Find the optimal type count */
            for (int k = 2; k <= sqrt(m_pGraph->vcount()); k++) {
                info(">> trying with %d types", k);
                fitForGivenGroupCount(k);
                currentAIC = aic(m_bestModel);
				currentBIC = bic(m_bestModel);
                if (currentAIC < bestAIC) {
                    bestAIC = currentAIC;
                    bestModel = m_bestModel;
                }
                if (currentBIC < bestBIC) {
                    bestBIC = currentBIC;
                }
                debug(">> AIC = %.4f (%.4f), BIC = %.4f (%.4f)",
						currentAIC, bestAIC, currentBIC, bestBIC);
            }

            *m_pModel = bestModel;
            m_bestModel = bestModel;
            m_bestLogL = m_pModel->getLogLikelihood();
            info(">> best type count is %d", m_pModel->getNumTypes());
        }

        /* Start sampling */
        if (m_args.numSamples > 0) {
            /* taking a finite number of samples */
            Vector samples;

            info(">> convergence condition satisfied, taking %d samples", m_args.numSamples);
            samples.reserve(m_args.numSamples);
            runBlock(m_args.numSamples, samples);
        } else {
            /* leave the Markov chain running anyway */
            info(">> convergence condition satisfied, leaving the chain running anyway");
#ifdef SIGUSR1
            info(">> send SIGUSR1 to dump the current best state to stdout");
#endif
            runUntilHellFreezesOver();
        }

        /* Dump the best solution found */
        dumpBestState();
        return 0;
    }
};

// Global app instance. It is here because we need it in the signal handler
BlockmodelFittingApp app;

/// Signal handler called for SIGUSR1
void handleSIGUSR1(int signum) {
    app.raiseDumpBestStateFlag();
}

int main(int argc, char** argv) {
#ifdef SIGUSR1
    // Register the signal handler for SIGUSR1
    signal(SIGUSR1, handleSIGUSR1);
#endif

    igraph::AttributeHandler::attach();
    return app.run(argc, argv);
}
