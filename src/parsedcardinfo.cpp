#include "parsedcardinfo.h"
#include <QRegularExpression>


namespace ygo {

    ParsedCardInfo::ParsedCardInfo(const CardInfo &other)
        : CardInfo(other),
          m_wordCount(0),
          m_charCount(0)
    {
        parseDescription();
    }

    QString ParsedCardInfo::simplifiedDescription() const {
        return m_simplifiedDescription;
    }

    void ParsedCardInfo::parseDescription() {
        static const QRegularExpression re_word("\\b\\w+\\b");
        m_simplifiedDescription = description().trimmed();

        auto it = re_word.globalMatch(m_simplifiedDescription);
        if (!it.isValid())
            return;

        while (it.hasNext()) {
            it.next();
            ++m_wordCount;
        }

        m_charCount = m_simplifiedDescription.length();
    }

} // namespace ygo
