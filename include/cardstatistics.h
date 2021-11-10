#ifndef CARDSTATISTICS_H
#define CARDSTATISTICS_H

#include "cardinfo.h"


namespace ygo {

    class CardStatistics {
    public:
        explicit CardStatistics(const CardInfo &card);
        CardStatistics(const CardStatistics &other) = default;
        CardStatistics(CardStatistics &&other) = default;
        CardStatistics &operator=(const CardStatistics &other) = default;
        CardStatistics &operator=(CardStatistics &&other) = default;

        QString name() const { return m_name; }
        QString description() const { return m_description; }
        QString simplifiedEffect() const { return m_simplifiedEffect; }
        CardType cardType() const { return m_cardType; }
        int wordCount() const { return m_wordCount; }
        int charCount() const { return m_charCount; }

    private:
        QString m_name;
        QString m_description;
        QString m_simplifiedEffect;
        CardType m_cardType;
        int m_wordCount;
        int m_charCount;
    };

} // namespace ygo

#endif // CARDSTATISTICS_H
