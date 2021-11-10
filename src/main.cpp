#include <iostream>
#include <sqlite3.h>
#include <QMap>
#include <QtMath>
#include <cmath>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QRegularExpression>
#include <algorithm>
#include <QDate>

#include "commandline.h"
#include "parseutil.h"
#include "cardinfo.h"
#include "cardstatistics.h"


static QStringList getIncludedDatabaseFiles(const QFileInfoList &dbFiles);
static QStringList getExcludedDatabaseFiles(const QFileInfoList &dbFiles);
static QMap<int, ygo::CardInfo> readCardInfoFromDatabase(const QString &file);
static int selectFromDatasTable(QMap<int, ygo::CardInfo> *data, int argc, char **argv, char **azColName);
static int selectFromTextsTable(QMap<int, ygo::CardInfo> *data, int argc, char **argv, char **azColName);
static QMultiMap<int, int> readExcludedCardIds(const QString &file);
static int selectFromExcludedDatasTable(QMultiMap<int, int> *idsByAlias, int argc, char **argv, char **azColName);
static int getCardLimitation(const QList<int> &ids,
                             const QMap<int, int> &prevLimits,
                             const QMap<int, int> &currentFormatLimits);
static QString getFormatName(double percentile);

typedef int (*execCallback)(void*, int, char**, char**);


int main(int argc, char *argv[]) {
    const QStringList args = getArguments(argc, argv);
    const CommandFlags flags = parseCommandFlags(args);
    if (flags.helpNeeded) {
        printHelp();
        return 1;
    }

    const QMap<int, int> previousCardLimits = parseLFListConf(flags.prevLFList);
    const QMap<int, int> currentFormatCardLimits = parseLFListConf(flags.currentFormatLFList);

    const QFileInfoList dbFiles = QDir(flags.dbPath).entryInfoList({ "*.cdb" }, QDir::Files);
    const QStringList dbIncludedFiles = getIncludedDatabaseFiles(dbFiles);
    const QStringList dbExcludedFiles = getExcludedDatabaseFiles(dbFiles);

    // Consolidate the entire legal cardpool from the databases and map them by id
    QMap<int, ygo::CardInfo> allCardsById;
    for (const auto &dbFile : dbIncludedFiles) {
        allCardsById.insert(readCardInfoFromDatabase(dbFile));
    }

    // Collect all card ids that may need to be excluded from the cardpool (rush cards, anime cards, etc.)
    QMultiMap<int, int> excludedIdsByAlias;
    for (const auto &dbFile : dbExcludedFiles) {
        const auto idsByAlias = readExcludedCardIds(dbFile);
        for (const auto alias : idsByAlias.keys()) {
            for (const auto id : idsByAlias.values(alias)) {
                excludedIdsByAlias.insert(alias, id);
            }
        }
    }

    // Remove tokens and pre-errata cards from the cardpool
    QMap<int, ygo::CardInfo> cardsById;
    for (const auto &card : allCardsById) {
        if (!(card.cardType() & ygo::Token || card.ot() == 8)) {
            cardsById.insert(card.id(), card);
        }
    }

    // Map card ids by card name to handle alt arts
    QMultiMap<QString, int> idsByName;
    for (const auto &card : cardsById) {
        idsByName.insert(card.name(), card.id());
    }

    // Split effect cards and non-effect cards into separate maps
    QMap<QString, ygo::CardInfo> effectCardsByName;
    QMap<QString, ygo::CardInfo> nonEffectCardsByName;
    for (const auto &card : cardsById) {
        if (card.alias() == 0) {
            if (card.hasEffect()) {
                effectCardsByName.insert(card.name(), ygo::CardInfo(card));
            } else {
                nonEffectCardsByName.insert(card.name(), ygo::CardInfo(card));
            }
        }
    }

    // Calculate statistics for effect cards and map them by card name
    QMap<QString, ygo::CardStatistics> effectCardStats;
    for (const auto &card : effectCardsByName) {
        effectCardStats.insert(card.name(), ygo::CardStatistics(card));
    }

    // Collect word and character counts and sort them
    std::vector<int> wordCounts;
    std::vector<int> charCounts;
    wordCounts.reserve(effectCardStats.count());
    charCounts.reserve(effectCardStats.count());
    for (const auto &effectCard : effectCardStats) {
        wordCounts.push_back(effectCard.wordCount());
        charCounts.push_back(effectCard.charCount());
    }
    std::sort(wordCounts.begin(), wordCounts.end());
    std::sort(charCounts.begin(), charCounts.end());

    // Find the specified percentile for both the word and character counts
    const int percentileIndex = round(effectCardsByName.count() * (flags.percentile / 100));
    const int wordPercentile = wordCounts.at(percentileIndex);
    const int charPercentile = charCounts.at(percentileIndex);

    // Collect the cards that exist in the percentile
    QMap<QString, ygo::CardInfo> cardsInPercentile;
    for (const auto &card : effectCardStats) {
        if (card.wordCount() <= wordPercentile && card.charCount() <= charPercentile) {
            cardsInPercentile.insert(card.name(), effectCardsByName[card.name()]);
        }
    }
    const int percentileEffectCards = cardsInPercentile.count();
    cardsInPercentile.insert(nonEffectCardsByName);

    std::cout << '\n';
    std::cout << "     Percentile word count: " << wordPercentile << '\n';
    std::cout << "     Percentile char count: " << charPercentile << '\n';
    std::cout << "        Total effect cards: " << effectCardsByName.count() << '\n';
    std::cout << "Effect cards in percentile: " << percentileEffectCards << '\n';
    std::cout << " Total cards in percentile: " << cardsInPercentile.count() << '\n';

    // Create the config file
    QFile conf(flags.outputLFList);
    if (!conf.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        std::cout << "Could not open the specified output file: " << flags.outputLFList.toStdString() << '\n';
        return 1;
    }

    // Write the cardpool to the config file
    QTextStream out(&conf);
    const auto name = getFormatName(flags.percentile);
    out << "#[" + name + "]\n!" + name + "\n$whitelist\n\n";

    QList<int> excludedIds;
    for (const auto &card : cardsInPercentile) {
        auto id = QString::number(card.id());
        const int padding = 8 - id.length();
        id = QString('0').repeated(padding) + id;
        const int limit = getCardLimitation(idsByName.values(card.name()), previousCardLimits, currentFormatCardLimits);
        out << id << ' ' << limit << " --" << card.name() << '\n';

        if (excludedIdsByAlias.contains(card.id())) {
            excludedIds.append(excludedIdsByAlias.values(card.id()));
        }
    }

    out << '\n';

    for (const auto id : excludedIds) {
        out << id << " -1\n";
    }

    out << Qt::flush;

    return 0;
}


static const QRegularExpression re_included(R"((^(cards.cdb|cards.delta.cdb)|.*\brelease\b.*\.cdb)$)");

inline QStringList getIncludedDatabaseFiles(const QFileInfoList &dbFiles) {
    QStringList dbIncludedFiles;

    for (const auto &file : dbFiles) {
        if (file.fileName().contains(re_included)) {
            dbIncludedFiles.append(file.filePath());
        }
    }

    return dbIncludedFiles;
}

inline QStringList getExcludedDatabaseFiles(const QFileInfoList &dbFiles) {
    QStringList dbExcludedFiles;

    for (const auto &file : dbFiles) {
        if (!file.fileName().contains(re_included)) {
            dbExcludedFiles.append(file.filePath());
        }
    }

    return dbExcludedFiles;
}

inline QMap<int, ygo::CardInfo> readCardInfoFromDatabase(const QString &file) {
    sqlite3 *db = nullptr;

    const int rc = sqlite3_open(file.toStdString().c_str(), &db);

    if (rc) {
        std::cerr << "Card database could not be opened: " << sqlite3_errmsg(db) << '\n';
        return {};
    }

    QMap<int, ygo::CardInfo> cards;

    char *errMsg = nullptr;
    sqlite3_exec(db, "select id,ot,alias,type from datas", (execCallback)selectFromDatasTable, &cards, &errMsg);

    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << errMsg << '\n';
        sqlite3_free(errMsg);
    }

    errMsg = nullptr;
    sqlite3_exec(db, "select id,name,desc from texts", (execCallback)selectFromTextsTable, &cards, &errMsg);

    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << errMsg << '\n';
        sqlite3_free(errMsg);
    }

    sqlite3_close(db);

    return cards;
}

inline int selectFromDatasTable(QMap<int, ygo::CardInfo> *cards, int argc, char **argv, char **azColName) {
    ygo::CardInfo card;

    for (int i = 0; i < argc; ++i) {
        QString colName(azColName[i]);
        QString value(argv[i] ? argv[i] : "NULL");

        if (colName == "id") {
            bool convertedSuccessfully = false;
            card.setId(value.toInt(&convertedSuccessfully));

            if (!convertedSuccessfully) {
                return 1;
            }
        } else if (colName == "ot") {
            bool convertedSuccessfully = false;
            card.setOt(value.toInt(&convertedSuccessfully));

            if (!convertedSuccessfully) {
                return 1;
            }
        } else if (colName == "alias") {
            bool convertedSuccessfully = false;
            card.setAlias(value.toInt(&convertedSuccessfully));

            if (!convertedSuccessfully) {
                return 1;
            }
        } else if (colName == "type") {
            bool convertedSuccessfully = false;
            card.setCardType(static_cast<ygo::CardType>(value.toInt(&convertedSuccessfully)));

            if (!convertedSuccessfully) {
                return 1;
            }
        }
    }

    if (cards->contains(card.id())) {
        (*cards)[card.id()].setOt(card.ot());
        (*cards)[card.id()].setAlias(card.alias());
        (*cards)[card.id()].setCardType(card.cardType());
    } else {
        cards->insert(card.id(), card);
    }

    return 0;
}

inline int selectFromTextsTable(QMap<int, ygo::CardInfo> *cards, int argc, char **argv, char **azColName) {
    ygo::CardInfo card;

    for (int i = 0; i < argc; ++i) {
        QString colName(azColName[i]);
        QString value(argv[i] ? argv[i] : "NULL");

        if (colName == "id") {
            bool convertedSuccessfully = false;
            card.setId(value.toInt(&convertedSuccessfully));

            if (!convertedSuccessfully) {
                return 1;
            }
        } else if (colName == "name") {
            card.setName(value);
        } else if (colName == "desc") {
            card.setDescription(value);
        }
    }

    if (cards->contains(card.id())) {
        (*cards)[card.id()].setName(card.name());
        (*cards)[card.id()].setDescription(card.description());
    } else {
        cards->insert(card.id(), card);
    }

    return 0;
}

inline QMultiMap<int, int> readExcludedCardIds(const QString &file) {
    sqlite3 *db = nullptr;

    const int rc = sqlite3_open(file.toStdString().c_str(), &db);

    if (rc) {
        std::cerr << "Card database could not be opened: " << sqlite3_errmsg(db) << '\n';
        return {};
    }

    QMultiMap<int, int> idsByAlias;

    char *errMsg = nullptr;
    sqlite3_exec(db, "select id,alias from datas", (execCallback)selectFromExcludedDatasTable, &idsByAlias, &errMsg);

    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << errMsg << '\n';
        sqlite3_free(errMsg);
    }

    sqlite3_close(db);

    return idsByAlias;
}

inline int selectFromExcludedDatasTable(QMultiMap<int, int> *idsByAlias, int argc, char **argv, char **azColName) {
    int id = 0;
    int alias = 0;

    for (int i = 0; i < argc; ++i) {
        QString colName(azColName[i]);
        QString value(argv[i] ? argv[i] : "NULL");

        if (colName == "id") {
            bool convertedSuccessfully = false;
            id = value.toInt(&convertedSuccessfully);

            if (!convertedSuccessfully) {
                return 1;
            }
        } else if (colName == "alias") {
            bool convertedSuccessfully = false;
            alias = value.toInt(&convertedSuccessfully);

            if (!convertedSuccessfully) {
                return 1;
            }
        }
    }

    if (alias != 0) {
        idsByAlias->insert(alias, id);
    }

    return 0;
}

inline int getCardLimitation(const QList<int> &ids,
                             const QMap<int, int> &prevLimits,
                             const QMap<int, int> &currentFormatLimits) {
    for (int id : ids) {
        if (prevLimits.contains(id)) {
            return prevLimits.value(id);
        }
    }

    for (int id : ids) {
        if (currentFormatLimits.contains(id)) {
            return currentFormatLimits.value(id);
        }
    }

    return 3;
}

inline QString getFormatName(double percentile) {
    auto name = QString::number(percentile);
    if (name.endsWith("1")) {
        name += "st";
    } else if (name.endsWith("2")) {
        name += "nd";
    } else if (name.endsWith("3")) {
        name += "rd";
    } else {
        name += "th";
    }

    return QString("%1.%2 %3").arg(QDate::currentDate().year()).arg(QDate::currentDate().month()).arg(name);
}
