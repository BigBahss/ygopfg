#include "cardinfo.h"


namespace ygo {

    CardInfo::CardInfo()
        : m_cardType(NullType),
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

    bool CardInfo::hasEffect() const {
        if (cardType() & ygo::Effect) {
            return true;
        } else if (cardType() & ygo::Spell) {
            if (cardType() & ygo::Pendulum) {
                if (description().contains("[ Pendulum Effect ]")) {
                    return true;
                } else {
                    return false;
                }
            } else {
                return true;
            }
            return true;
        } else if (cardType() & ygo::Trap) {
            return true;
        } else {
            return false;
        }
    }

}
