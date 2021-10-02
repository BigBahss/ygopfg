#ifndef PARSEDCARDINFO_H
#define PARSEDCARDINFO_H

#include "cardinfo.h"


namespace ygo {

    class ParsedCardInfo : public CardInfo {
    public:
        explicit ParsedCardInfo(const CardInfo &other);

        explicit ParsedCardInfo(const ParsedCardInfo &other) = default;
        explicit ParsedCardInfo(ParsedCardInfo &&other) = default;
        ParsedCardInfo &operator=(const ParsedCardInfo &other) = default;
        ParsedCardInfo &operator=(ParsedCardInfo &&other) = default;

        void parseDescription();

        QString simplifiedDescription() const;
        int wordCount() const { return m_wordCount; }
        int charCount() const { return m_charCount; }

    private:
        QString m_simplifiedDescription;
        int m_wordCount;
        int m_charCount;
    };

} // namespace ygo

#endif // PARSEDCARDINFO_H
