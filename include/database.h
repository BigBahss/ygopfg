#include <QString>
#include <QList>
#include <QMap>
#include <QFileInfo>

#include "cardinfo.h"


QStringList getIncludedDatabaseFiles(const QFileInfoList &dbFiles);
QStringList getExcludedDatabaseFiles(const QFileInfoList &dbFiles);
QMap<int, ygo::CardInfo> readCardInfoFromDatabase(const QString &file);
QMultiMap<int, int> readExcludedCardIds(const QString &file);
