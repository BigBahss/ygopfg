#include "cardinfo.h"


namespace ygo {

    CardInfo::CardInfo()
        : m_cardType(NullType),
          m_id(0),
          m_ot(0)
    {

    }

    void CardInfo::setId(int id) {
        m_id = id;
    }

    void CardInfo::setOt(int ot) {
        m_ot = ot;
    }

    void CardInfo::setAlias(int alias) {
        m_alias = alias;
    }

    void CardInfo::setCardType(CardType cardType) {
        m_cardType = cardType;
    }

    void CardInfo::setName(const QString &name) {
        m_name = name;
    }

    void CardInfo::setDescription(const QString &description) {
        m_description = description;
    }

    bool CardInfo::hasEffect() const {
        if (cardType() & ygo::Effect || cardType() & ygo::Spell || cardType() & ygo::Trap) {
            return true;
        } else if (cardType() & ygo::Pendulum) {
            // Only normal pendulums will reach here, so we only check if the card has a pendulum effect
            if (description().contains("[ Pendulum Effect ]")) {
                return true;
            } else {
                return false;
            }
        } else {
            return false;
        }
    }

}
