/* vim:set ts=4 sw=4 sts=4 et: */

#include <ctime>
#include <iostream>
#include <string>
#include <block/version.h>

#include "cmd_arguments.h"

using namespace std;
using namespace SimpleOpt;

enum {
    NUM_GROUPS, NUM_SAMPLES, OUT_FORMAT,
    LOG_PERIOD, INIT_METHOD, BLOCK_SIZE
};

CommandLineArguments::CommandLineArguments() :
    CommandLineArgumentsBase("block-fit", BLOCKMODEL_VERSION_STRING),
    numGroups(-1), numSamples(100000), outputFormat(FORMAT_PLAIN),
    blockSize(65536), initMethod(GREEDY), logPeriod(8192) {

    /* basic options */

    addOption(OUT_FORMAT,  "-F", SO_REQ_SEP, "--out-format");
    addOption(NUM_GROUPS,  "-g", SO_REQ_SEP, "--groups");
    addOption(NUM_SAMPLES, "-s", SO_REQ_SEP, "--samples");

    /* advanced options */
    addOption(BLOCK_SIZE,  "--block-size",  SO_REQ_SEP);
    addOption(INIT_METHOD, "--init-method", SO_REQ_SEP);
    addOption(LOG_PERIOD,  "--log-period",  SO_REQ_SEP);
}

int CommandLineArguments::handleOption(int id, const std::string& arg) {
    switch (id) {
        /* Processing basic algorithm parameters */

        case NUM_GROUPS:
            numGroups = atoi(arg.c_str());
            break;

        case NUM_SAMPLES:
            numSamples = atol(arg.c_str());
            break;

        case OUT_FORMAT:
            if (arg == "plain")
                outputFormat = FORMAT_PLAIN;
            else if (arg == "json")
                outputFormat = FORMAT_JSON;
            else if (arg == "null")
                outputFormat = FORMAT_NULL;
            else {
                cerr << "Unknown output format: " << arg << "\n";
                return 1;
            }
            break;

        /* Processing advanced parameters */

        case BLOCK_SIZE:
            blockSize = atoi(arg.c_str());
            break;

        case INIT_METHOD:
            if (arg == "greedy")
                initMethod = GREEDY;
            else if (arg == "random")
                initMethod = RANDOM;
            else {
                cerr << "Unknown initialization method: " << arg << '\n';
                return 1;
            }
            break;

        case LOG_PERIOD:
            logPeriod = atoi(arg.c_str());
            break;

    }

    return 0;
}

void CommandLineArguments::showHelp(ostream& os) const {
    CommandLineArgumentsBase::showHelp(os);
    os << "Basic algorithm parameters:\n"
          "    -F FORMAT, --output-format FORMAT\n"
          "                        sets the format of the output file. The default value\n"
          "                        is plain, which is a simple plain text format. Known\n"
          "                        formats are: json, plain.\n"
          "    -g K, --groups K    sets the desired number of groups to\n"
          "                        K. Default = -1 (autodetection).\n"
          "    -o FILE, --output FILE\n"
          "                        sets the name of the output file where the results\n"
          "                        will be written. The default is the standard\n"
          "                        output stream.\n"
          "    -s N, --samples N   sets the number of samples to be taken from the\n"
          "                        Markov chain after convergence. The default is 100000.\n"
          "\n"
          "Advanced algorithm parameters:\n"
          "    --block-size N      sets the block size used when determining the\n"
          "                        convergence of the Markov chain to N. The default is\n"
          "                        10000 samples.\n"
          "    --init-method METH  use the given initialization method METH for\n"
          "                        the Markov chain. Available methods: greedy (default),\n"
          "                        random.\n"
          "    --log-period COUNT  shows a status message after every COUNT steps.\n"
          "                        The default value is 8192.\n"
          "    --model MODEL       selects the type of the model being fitted.\n"
          "                        Available models: uncorrected (default), degree.\n"
          "    --seed SEED         use the given number to seed the random number\n"
          "                        generator.\n"
    ;
}


