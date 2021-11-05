#ifndef COMMANDLINE_H
#define COMMANDLINE_H

#include <QString>
#include <QList>


struct CommandFlags {
    QString dbPath;
    QString outputLFList;
    QString prevLFList;
    QString currentFormatLFList;
    bool helpNeeded = false;
};

QStringList getArguments(int argc, char *argv[]);
CommandFlags parseCommandFlags(const QStringList &args);
void printHelp();

#endif // COMMANDLINE_H
