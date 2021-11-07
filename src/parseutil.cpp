#include "parseutil.h"
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <iostream>


QString readTextFile(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        std::cout << "Could not open file: " << path.toStdString() << '\n';
        return QString();
    }

    QTextStream in(&file);
    in.setCodec("UTF-8");
    QString text = "";
    while (!in.atEnd()) {
        text += in.readLine().trimmed() + '\n';
    }

    return text;
}


QMap<int, int> parseLFListConf(const QString &path) {
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

    QMap<int, int> cardLimitsById;
    static const QRegularExpression re_lfEntry(R"(^\s*(?<id>\d+)\s+(?<limit>\d)\s+--.*$)", QRegularExpression::MultilineOption);
    auto it = re_lfEntry.globalMatch(text);
    while (it.hasNext()) {
        const auto entry = it.next();
        const int id = entry.captured("id").toInt();
        const int limit = entry.captured("limit").toInt();
        cardLimitsById.insert(id, limit);
    }

    return cardLimitsById;
}
