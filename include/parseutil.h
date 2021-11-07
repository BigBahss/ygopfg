#ifndef PARSEUTIL_H
#define PARSEUTIL_H

#include <QString>
#include <QMap>


// Returns the text contents of the file at path
QString readTextFile(const QString &path);

// Parses the lflist.conf file at the given path and returns the card limitations mapped by card id
QMap<int, int> parseLFListConf(const QString &path);

#endif // PARSEUTIL_H
