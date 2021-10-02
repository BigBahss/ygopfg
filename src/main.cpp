#include <iostream>
#include <sqlite3.h>
#include <QMap>

#include <cardinfo.h>
#include <parsedcardinfo.h>


static QStringList getArguments(int argc, char *argv[]);
static QString getDatabaseFile(const QStringList &args);

static int selectFromDatasTable(QMap<int, ygo::CardInfo> *data, int argc, char **argv, char **azColName);
static int selectFromTextsTable(QMap<int, ygo::CardInfo> *data, int argc, char **argv, char **azColName);

typedef int (*execCallback)(void*, int, char**, char**);


int main(int argc, char *argv[]) {
    QStringList args = getArguments(argc, argv);
    QString file = getDatabaseFile(args);
    if (file.isEmpty()) {
        return 1;
    }

    sqlite3 *db;

    int rc = sqlite3_open(file.toStdString().c_str(), &db);

    if (rc) {
        std::cerr << "Can't open card database: " << sqlite3_errmsg(db) << '\n';
        return rc;
    } else {
        std::cout << "Opened card database successfully\n\n";
    }

    QMap<int, ygo::CardInfo> cards;

    char *errMsg = nullptr;
    sqlite3_exec(db, "select id,type from datas", (execCallback)selectFromDatasTable, &cards, &errMsg);

    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << errMsg << '\n';
        sqlite3_free(errMsg);
    } else {
        std::cout << "Operation completed successfully\n";
    }

    errMsg = nullptr;
    sqlite3_exec(db, "select id,name,desc from texts", (execCallback)selectFromTextsTable, &cards, &errMsg);

    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << errMsg << '\n';
        sqlite3_free(errMsg);
    } else {
        std::cout << "Operation completed successfully\n";
    }

    sqlite3_close(db);

    QMap<int, ygo::ParsedCardInfo> parsedCards;

    for (const auto &card : cards) {
        parsedCards.insert(card.id(), ygo::ParsedCardInfo(card));
    }
}

inline QStringList getArguments(int argc, char *argv[]) {
    QStringList args;
    for (int i = 0; i < argc; ++i) {
        args.append(argv[i]);
    }
    return args;
}

inline QString getDatabaseFile(const QStringList &args) {
    if (args.count() > 1) {
        return args.last();
    } else {
        std::cout << "Please specify a card database file\n";
        return QString();
    }
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

        std::cout << colName.toStdString() << " = " << value.toStdString() << '\n';
    }
    std::cout << '\n';

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

        std::cout << colName.toStdString() << " = " << value.toStdString() << '\n';
    }
    std::cout << '\n';

    if (cards->contains(card.id())) {
        (*cards)[card.id()].setName(card.name());
        (*cards)[card.id()].setDescription(card.description());
    } else {
        cards->insert(card.id(), card);
    }

    return 0;
}
