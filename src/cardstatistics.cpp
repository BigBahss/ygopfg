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
