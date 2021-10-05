#include <iostream>
#include <sqlite3.h>
#include <QMap>

#include <cardinfo.h>
#include <parsedcardinfo.h>
#include <QtMath>


static QStringList getArguments(int argc, char *argv[]);
static QString getDatabaseFileFromArguments(const QStringList &args);
static QMap<int, ygo::CardInfo> readCardInfoFromDatabase(const QString &file);

static int selectFromDatasTable(QMap<int, ygo::CardInfo> *data, int argc, char **argv, char **azColName);
static int selectFromTextsTable(QMap<int, ygo::CardInfo> *data, int argc, char **argv, char **azColName);

typedef int (*execCallback)(void*, int, char**, char**);


int main(int argc, char *argv[]) {
    QStringList args = getArguments(argc, argv);
    QString file = getDatabaseFileFromArguments(args);
    if (file.isEmpty()) {
        return 1;
    }

    QMap<int, ygo::CardInfo> cards = readCardInfoFromDatabase(file);
    if (cards.isEmpty()) {
        return 1;
    }

    QMap<int, ygo::ParsedCardInfo> parsedCards;

    for (const auto &card : cards)
        parsedCards.insert(card.id(), ygo::ParsedCardInfo(card));

    int cardTotal = parsedCards.count();

    // Calculate the means
    double wordTotal = 0;
    double charTotal = 0;
    for (const auto &parsedCard : parsedCards) {
        wordTotal += parsedCard.wordCount();
        charTotal += parsedCard.charCount();
    }
    double wordMean = wordTotal / cardTotal;
    double charMean = charTotal / cardTotal;

    // Calculate the standard deviations
    double wordSqrDistTotal = 0;
    double charSqrDistTotal = 0;
    for (const auto &parsedCard : parsedCards) {
        wordSqrDistTotal += qPow(parsedCard.wordCount() - wordMean, 2);
        charSqrDistTotal += qPow(parsedCard.charCount() - charMean, 2);
    }
    double wordStdDev = qSqrt(wordSqrDistTotal / cardTotal);
    double charStdDev = qSqrt(charSqrDistTotal / cardTotal);

    // Calculate 25th percentiles
    double word25th = wordMean + (-0.675) * wordStdDev;
    double char25th = charMean + (-0.675) * charStdDev;

    std::cout << '\n';
    std::cout << "                     Total cards: " << cardTotal << '\n';
    std::cout << "                 Mean word count: " << wordMean << '\n';
    std::cout << "                 Mean char count: " << charMean << '\n';
    std::cout << "Standard deviation of word count: " << wordStdDev << '\n';
    std::cout << "Standard deviation of char count: " << charStdDev << '\n';
    std::cout << "      25th percentile word count: " << word25th << '\n';
    std::cout << "      25th percentile char count: " << char25th << '\n';
}

inline QStringList getArguments(int argc, char *argv[]) {
    QStringList args;
    for (int i = 0; i < argc; ++i) {
        args.append(argv[i]);
    }
    return args;
}

inline QString getDatabaseFileFromArguments(const QStringList &args) {
    if (args.count() > 1) {
        return args.last();
    } else {
        std::cout << "Please specify a card database file\n";
        return QString();
    }
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
    sqlite3_exec(db, "select id,type from datas", (execCallback)selectFromDatasTable, &cards, &errMsg);

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
    bool cardTypeConvertedSuccessfully = false;

    for (int i = 0; i < argc; ++i) {
        QString colName(azColName[i]);
        QString value(argv[i] ? argv[i] : "NULL");

        if (colName == "id") {
            card.setId(value.toInt(&idConvertedSuccessfully));

            if (!idConvertedSuccessfully) {
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
