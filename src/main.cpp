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

#include "commandline.h"
#include "parseutil.h"
#include "cardinfo.h"
#include "cardstatistics.h"


static QStringList getCardDatabaseFiles(const QString &dbPath);
static QMap<int, ygo::CardInfo> readCardInfoFromDatabase(const QString &file);
static int selectFromDatasTable(QMap<int, ygo::CardInfo> *data, int argc, char **argv, char **azColName);
static int selectFromTextsTable(QMap<int, ygo::CardInfo> *data, int argc, char **argv, char **azColName);
static int getCardLimitation(const QList<int> &ids,
                             const QMap<int, int> &prevLimits,
                             const QMap<int, int> &currentFormatLimits);

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

    const QStringList dbFiles = getCardDatabaseFiles(flags.dbPath);

    // Consolidate the entire legal cardpool from the databases and map them by id
    QMap<int, ygo::CardInfo> allCardsById;
    for (const auto &dbFile : dbFiles) {
        allCardsById.insert(readCardInfoFromDatabase(dbFile));
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
        if (card.hasEffect()) {
            effectCardsByName.insert(card.name(), ygo::CardInfo(card));
        } else {
            nonEffectCardsByName.insert(card.name(), ygo::CardInfo(card));
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

    // Find the 25th percentile for both the word and character counts
    int percentileIndex = round(effectCardsByName.count() * 0.25);
    int word25th = wordCounts.at(percentileIndex);
    int char25th = charCounts.at(percentileIndex);

    // Collect the cards that exist in the percentile
    QMap<QString, ygo::CardInfo> cardsInPercentile;
    for (const auto &card : effectCardStats) {
        if (card.wordCount() <= word25th && card.charCount() <= char25th) {
            cardsInPercentile.insert(card.name(), effectCardsByName[card.name()]);
        }
    }
    int percentileEffectCards = cardsInPercentile.count();
    cardsInPercentile.insert(nonEffectCardsByName);

    std::cout << '\n';
    std::cout << "     Percentile word count: " << word25th << '\n';
    std::cout << "     Percentile char count: " << char25th << '\n';
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
    out << "#[2021.9 25th]\n!2021.9 25th\n$whitelist\n\n";
    for (const auto &card : cardsInPercentile) {
        auto id = QString::number(card.id());
        const int padding = 8 - id.length();
        id = QString('0').repeated(padding) + id;
        const int limit = getCardLimitation(idsByName.values(card.name()), previousCardLimits, currentFormatCardLimits);
        out << id << ' ' << limit << " --" << card.name() << '\n';
    }
    out << Qt::flush;

    return 0;
}

inline QStringList getCardDatabaseFiles(const QString &dbPath) {
    const QDir directory(dbPath);

    QStringList dbFiles;

    if (!directory.exists()) {
        return dbFiles;
    }

    static const QRegularExpression re_cardDatabase(R"((^(cards.cdb|cards.delta.cdb)|\brelease\b.*\.cdb)$)");

    const auto files = directory.entryInfoList({ "*.cdb" }, QDir::Files, QDir::Size);

    for (const auto &file : files) {
        if (file.fileName().contains(re_cardDatabase)) {
            dbFiles.append(file.filePath());
        }
    }

    return dbFiles;
}

inline QMap<int, ygo::CardInfo> readCardInfoFromDatabase(const QString &file) {
    sqlite3 *db;

    int rc = sqlite3_open(file.toStdString().c_str(), &db);

    if (rc) {
        std::cerr << "Card database could not be opened: " << sqlite3_errmsg(db) << '\n';
        return {};
    } else {
        std::cout << "Card database opened successfully\n";
    }

    QMap<int, ygo::CardInfo> cards;

    char *errMsg = nullptr;
    sqlite3_exec(db, "select id,ot,type from datas", (execCallback)selectFromDatasTable, &cards, &errMsg);

    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << errMsg << '\n';
        sqlite3_free(errMsg);
    } else {
        std::cout << "Read id and type from datas table successfully\n";
    }

    errMsg = nullptr;
    sqlite3_exec(db, "select id,name,desc from texts", (execCallback)selectFromTextsTable, &cards, &errMsg);

    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << errMsg << '\n';
        sqlite3_free(errMsg);
    } else {
        std::cout << "Read id, name, and desc from texts table successfully\n";
    }

    sqlite3_close(db);
    std::cout << "Closed card database\n";

    return cards;
}

inline int selectFromDatasTable(QMap<int, ygo::CardInfo> *cards, int argc, char **argv, char **azColName) {
    ygo::CardInfo card;
    bool idConvertedSuccessfully = false;
    bool otConvertedSuccessfully = false;
    bool cardTypeConvertedSuccessfully = false;

    for (int i = 0; i < argc; ++i) {
        QString colName(azColName[i]);
        QString value(argv[i] ? argv[i] : "NULL");

        if (colName == "id") {
            card.setId(value.toInt(&idConvertedSuccessfully));

            if (!idConvertedSuccessfully) {
                return 1;
            }
        } else if (colName == "ot") {
            card.setOt(static_cast<ygo::CardType>(value.toInt(&otConvertedSuccessfully)));

            if (!otConvertedSuccessfully) {
                return 1;
            }
        } else if (colName == "type") {
            card.setCardType(static_cast<ygo::CardType>(value.toInt(&cardTypeConvertedSuccessfully)));

            if (!cardTypeConvertedSuccessfully) {
                return 1;
            }
        }
    }

    if (cards->contains(card.id())) {
        (*cards)[card.id()].setCardType(card.cardType());
        (*cards)[card.id()].setOt(card.ot());
    } else {
        cards->insert(card.id(), card);
    }

    return 0;
}

inline int selectFromTextsTable(QMap<int, ygo::CardInfo> *cards, int argc, char **argv, char **azColName) {
    ygo::CardInfo card;
    bool idConvertedSuccessfully = false;

    for (int i = 0; i < argc; ++i) {
        QString colName(azColName[i]);
        QString value(argv[i] ? argv[i] : "NULL");

        if (colName == "id") {
            card.setId(value.toInt(&idConvertedSuccessfully));

            if (!idConvertedSuccessfully) {
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
