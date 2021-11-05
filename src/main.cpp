#include <iostream>
#include <sqlite3.h>
#include <QMap>
#include <QtMath>
#include <cmath>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QRegularExpression>

#include "commandline.h"
#include "cardinfo.h"
#include "cardstatistics.h"


static QStringList getCardDatabaseFiles(const QString &dbPath);
static QMap<int, ygo::CardInfo> readCardInfoFromDatabase(const QString &file);
static int selectFromDatasTable(QMap<int, ygo::CardInfo> *data, int argc, char **argv, char **azColName);
static int selectFromTextsTable(QMap<int, ygo::CardInfo> *data, int argc, char **argv, char **azColName);

typedef int (*execCallback)(void*, int, char**, char**);


int main(int argc, char *argv[]) {
    const QStringList args = getArguments(argc, argv);
    const CommandFlags flags = parseCommandFlags(args);
    if (flags.helpNeeded) {
        printHelp();
        return 1;
    }

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

    int effectCardTotal = effectCardsByName.count();

    // Calculate the means
    double wordTotal = 0;
    double charTotal = 0;
    for (const auto &effectCard : effectCardStats) {
        wordTotal += effectCard.wordCount();
        charTotal += effectCard.charCount();
    }
    double wordMean = wordTotal / effectCardTotal;
    double charMean = charTotal / effectCardTotal;

    // Calculate the standard deviations
    double wordSqrDistTotal = 0;
    double charSqrDistTotal = 0;
    for (const auto &effectCard : effectCardStats) {
        wordSqrDistTotal += qPow(effectCard.wordCount() - wordMean, 2);
        charSqrDistTotal += qPow(effectCard.charCount() - charMean, 2);
    }
    double wordStdDev = qSqrt(wordSqrDistTotal / effectCardTotal);
    double charStdDev = qSqrt(charSqrDistTotal / effectCardTotal);

    // Calculate 25th percentiles
    double word25th = wordMean + (-0.675) * wordStdDev;
    double char25th = charMean + (-0.675) * charStdDev;

    std::cout << '\n';
    std::cout << "              Total effect cards: " << effectCardTotal << '\n';
    std::cout << "                 Mean word count: " << wordMean << '\n';
    std::cout << "                 Mean char count: " << charMean << '\n';
    std::cout << "Standard deviation of word count: " << wordStdDev << '\n';
    std::cout << "Standard deviation of char count: " << charStdDev << '\n';
    std::cout << "      25th percentile word count: " << word25th << '\n';
    std::cout << "      25th percentile char count: " << char25th << '\n';

    int word25thRounded = round(word25th);
    int char25thRounded = round(char25th);

    // Collect the cards that exist in the percentile
    QMap<QString, ygo::CardInfo> cardsInPercentile(nonEffectCardsByName);
    for (const auto &card : effectCardStats) {
        if (card.wordCount() <= word25thRounded && card.charCount() <= char25thRounded) {
            cardsInPercentile.insert(card.name(), effectCardsByName[card.name()]);
        }
    }

    // Create the config file
    QFile conf(flags.outputLFList);
    conf.open(QIODevice::WriteOnly | QIODevice::Truncate);
    QTextStream fileStream(&conf);
    fileStream << "#[2021.9 25th]\n!2021.9 25th\n$whitelist\n\n";
    for (const auto &card : cardsInPercentile) {
        auto id = QString::number(card.id());
        const int padding = 8 - id.length();
        id = QString('0').repeated(padding) + id;
        fileStream << id << " 3 --" << card.name() << '\n';
    }
    fileStream << Qt::flush;

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
