#include <iostream>
#include <cmath>
#include <algorithm>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDate>
#include <QtMath>

#include "commandline.h"
#include "database.h"
#include "parseutil.h"
#include "cardinfo.h"
#include "cardstatistics.h"


static int getCardLimitation(const QList<int> &ids,
                             const QMap<int, int> &prevLimits,
                             const QMap<int, int> &currentFormatLimits);
static QString getFormatName(double percentile);


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


int getCardLimitation(const QList<int> &ids,
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

QString getFormatName(double percentile) {
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
