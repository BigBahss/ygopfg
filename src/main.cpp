#include <iostream>
#include <cmath>
#include <algorithm>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDate>
#include <QtMath>
#include <QRegularExpression>

#include "commandline.h"
#include "database.h"
#include "parseutil.h"
#include "cardinfo.h"
#include "cardstatistics.h"


QList<int> getExcludedIdsFromLFList(const QString &path);
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

    // Lambda function that returns the config line for a given card (Ex: 67284107 1 --Scapeghost)
    const auto createConfigLine = [&](const ygo::CardInfo &card) {
        auto id = QString::number(card.id());
        const int padding = 8 - id.length();
        id = QString('0').repeated(padding) + id;
        const int limit = getCardLimitation(idsByName.values(card.name()), previousCardLimits, currentFormatCardLimits);
        return QString(id + ' '+ "%1" + " --" + card.name()).arg(limit);
    };

    // Collect all ids of excluded versions of cards (rush cards, anime cards, etc.)
    QList<int> excludedIds;
    for (const auto &card : cardsInPercentile) {
        out << createConfigLine(card) << '\n';

        if (excludedIdsByAlias.contains(card.id())) {
            excludedIds.append(excludedIdsByAlias.values(card.id()));
        }
    }

    // Write the excluded ids to the config file
    if (excludedIds.count()) {
        out << '\n';
    }
    for (const auto id : excludedIds) {
        out << id << " -1\n";
    }

    // Return early if no previous lflist was given
    if (flags.prevLFList.isEmpty()) {
        out << Qt::flush;
        return 0;
    }

    QList<int> notIncludedInPrevious;
    QList<int> notIncludedInNew;
    for (const auto &card : cardsInPercentile) {
        if (!previousCardLimits.keys().contains(card.id())) {
            notIncludedInPrevious.append(card.id());
        }
    }
    for (const auto id : previousCardLimits.keys()) {
        bool found = false;
        for (const auto &card : cardsInPercentile) {
            if (card.id() == id) {
                found = true;
                break;
            }
        }
        if (!found) {
            notIncludedInNew.append(id);
        }
    }

    int newCardCount = notIncludedInPrevious.count();
    int removedCardCount = notIncludedInNew.count();
    if (newCardCount == 0 && removedCardCount == 0) {
        return 0;
    }

    std::cout << '\n';
    std::cout << "    Amount of cards added to cardpool: " << newCardCount << '\n';
    std::cout << "Amount of cards removed from cardpool: " << removedCardCount << '\n';

    const auto previousExcludedIds = getExcludedIdsFromLFList(flags.prevLFList);

    QList<int> notExcludedInPrevious;
    QList<int> notExcludedInNew;
    for (const auto id : excludedIds) {
        if (!previousExcludedIds.contains(id)) {
            notExcludedInPrevious.append(id);
        }
    }
    for (const auto id : previousExcludedIds) {
        if (!excludedIds.contains(id)) {
            notExcludedInNew.append(id);
        }
    }

    QMap<QString, ygo::CardInfo> newCards;
    for (const auto id : notIncludedInPrevious) {
        if (cardsById.contains(id)) {
            const auto &card = cardsById[id];
            newCards.insert(card.name(), card);
        }
    }
    QMap<QString, ygo::CardInfo> removedCards;
    for (const auto id : notIncludedInNew) {
        if (cardsById.contains(id)) {
            const auto &card = cardsById[id];
            removedCards.insert(card.name(), card);
        }
    }

    out << "\n## Cards added\n";
    for (const auto &card : newCards) {
        out << "# " << createConfigLine(card) << '\n';
    }

    out << "\n## Cards removed\n";
    for (const auto &card : removedCards) {
        out << "# " << createConfigLine(card) << '\n';
    }

    out << Qt::flush;

    return 0;
}


QList<int> getExcludedIdsFromLFList(const QString &path) {
    auto text = readTextFile(path);
    if (text.isEmpty()) {
        return {};
    }

    static const QRegularExpression re_lineComment(R"(^\s*#.*$)", QRegularExpression::MultilineOption);
    text.remove(re_lineComment);

    static const QRegularExpression re_shebang(R"(^\s*!.*$)", QRegularExpression::MultilineOption);
    text.remove(re_shebang);

    static const QRegularExpression re_whitelist(R"(^\s*\$whitelist\b.*$)", QRegularExpression::MultilineOption);
    text.remove(re_whitelist);

    static const QRegularExpression re_blankLines(R"(\n(\s*\n)+)");
    text.replace(re_blankLines, "\n");

    text = text.trimmed() + '\n';

    QList<int> excludedIds;
    static const QRegularExpression re_lfEntry(R"(^\s*(?<id>\d+)\s+-1.*$)", QRegularExpression::MultilineOption);
    auto it = re_lfEntry.globalMatch(text);
    while (it.hasNext()) {
        const auto entry = it.next();
        const int id = entry.captured("id").toInt();
        excludedIds.push_back(id);
    }

    return excludedIds;
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
