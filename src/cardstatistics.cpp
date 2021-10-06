#include "cardstatistics.h"
#include <QRegularExpression>


namespace ygo {

    CardStatistics::CardStatistics(const CardInfo &card)
        : m_name(card.name()),
          m_description(card.description()),
          m_simplifiedEffect(card.description()),
          m_cardType(card.cardType()),
          m_wordCount(0),
          m_charCount(0)
    {
        // Newlines aren't considered for the character count, so we remove them.
        static const QRegularExpression re_newLines(R"([\r\n])");
        m_simplifiedEffect.remove(re_newLines);

        static const QRegularExpression re_repeatedWhitespace(R"( {2,})");
        m_simplifiedEffect.replace(re_repeatedWhitespace, " ");
        m_simplifiedEffect = m_simplifiedEffect.trimmed();

        // Count words and characters
        static const QRegularExpression re_word(R"(\b\w+\b)");
        auto it = re_word.globalMatch(m_simplifiedEffect);
        if (!it.isValid()) {
            return;
        }

        while (it.hasNext()) {
            it.next();
            ++m_wordCount;
        }

        m_charCount = m_simplifiedEffect.length();
    }

} // namespace ygo
