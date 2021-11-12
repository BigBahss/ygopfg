#include "database.h"

#include <sqlite3.h>
#include <iostream>
#include <QRegularExpression>


static int selectFromDatasTable(QMap<int, ygo::CardInfo> *data, int argc, char **argv, char **azColName);
static int selectFromTextsTable(QMap<int, ygo::CardInfo> *data, int argc, char **argv, char **azColName);
static int selectFromExcludedDatasTable(QMultiMap<int, int> *idsByAlias, int argc, char **argv, char **azColName);

typedef int (*execCallback)(void*, int, char**, char**);

static const QRegularExpression re_included(R"((^(cards.cdb|cards.delta.cdb)|.*\brelease\b.*\.cdb)$)");


QStringList getIncludedDatabaseFiles(const QFileInfoList &dbFiles) {
    QStringList dbIncludedFiles;

    for (const auto &file : dbFiles) {
        if (file.fileName().contains(re_included)) {
            dbIncludedFiles.append(file.filePath());
        }
    }

    return dbIncludedFiles;
}

QStringList getExcludedDatabaseFiles(const QFileInfoList &dbFiles) {
    QStringList dbExcludedFiles;

    for (const auto &file : dbFiles) {
        if (!file.fileName().contains(re_included)) {
            dbExcludedFiles.append(file.filePath());
        }
    }

    return dbExcludedFiles;
}

QMap<int, ygo::CardInfo> readCardInfoFromDatabase(const QString &file) {
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

QMultiMap<int, int> readExcludedCardIds(const QString &file) {
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
