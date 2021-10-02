#include "cardinfo.h"


namespace ygo {

    CardInfo::CardInfo()
        : m_cardType(Null),
          m_id(0)
    {

    }

    void CardInfo::setName(const QString &name) {
        m_name = name;
    }

    void CardInfo::setCardType(CardType cardType) {
        m_cardType = cardType;
    }

    void CardInfo::setDescription(const QString &description) {
        m_description = description;
    }

    void CardInfo::setId(int id) {
        m_id = id;
    }

}
