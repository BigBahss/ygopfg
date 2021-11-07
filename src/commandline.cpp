#include "commandline.h"

#include <iostream>


QStringList getArguments(int argc, char *argv[]) {
    QStringList args;
    for (int i = 0; i < argc; ++i) {
        args.append(argv[i]);
    }
    return args;
}

CommandFlags parseCommandFlags(const QStringList &args) {
    CommandFlags flags;

    if (args.length() < 2) {
        flags.helpNeeded = true;
        return flags;
    }

    for (int i = 1; i < args.length(); ++i) {
        if (args.at(i) == "-p" && i < args.length() - 1) {
            bool convertedSuccessfully = false;
            const double percentile = args.at(++i).toDouble(&convertedSuccessfully);
            if (!convertedSuccessfully || percentile < 0 || percentile > 100) {
                flags.helpNeeded = true;
                break;
            }
            flags.percentile = percentile;
        } else if (args.at(i) == "-d" && i < args.length() - 1) {
            flags.dbPath = args.at(++i);
        } else if (args.at(i) == "-o" && i < args.length() - 1) {
            flags.outputLFList = args.at(++i);
        } else if (args.at(i) == "-l" && i < args.length() - 1) {
            flags.prevLFList = args.at(++i);
        } else if (args.at(i) == "-c" && i < args.length() - 1) {
            flags.currentFormatLFList = args.at(++i);
        } else {
            flags.helpNeeded = true;
            break;
        }
    }

    if (flags.dbPath.isEmpty() || flags.outputLFList.isEmpty()) {
        flags.helpNeeded = true;
    }

    return flags;
}

void printHelp() {
    std::cout << R"(
REQUIRED arguments:
  -p <%>        Specify a postive percentile value (from 0-100) to use as the
                  cutoff for the cardpool.
  -d <path>     Specify the directory of EDOPro's card database files, most
                  likely found in "repositories/delta-utopia/" within your
                  EDOPro installation directory.
  -o <file>     Specify the file name to output the EDOPro lflist (.conf file).
                  NOTE: If a file already exists in the specified location, it
                  will be overwritten.

OPTIONAL arguments:
  -l <file>     Specify a previous EDOPro lflist (.conf file) to reference in
                  order to carry over card limitations.
  -c <file>     Specify a current format EDOPro lflist (.conf file) to reference
                  in order to retrieve default card limitations for when new
                  cards get added to the cardpool that do not appear in previous
                  lflist (the file specified with [-p]).
)" << std::endl;
}
