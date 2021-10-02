#ifndef CARDINFO_H
#define CARDINFO_H

#include <QFlag>
#include <QString>


namespace ygo {

    enum CardTypeFlags {
        Null        = 0x00000000,
        Monster     = 0x00000001,
        Spell       = 0x00000002,
        Trap        = 0x00000004,
        Normal      = 0x00000010,
        Effect      = 0x00000020,
        Fusion      = 0x00000040,
        Ritual      = 0x00000080,
        TrapMonster = 0x00000100,
        Spirit      = 0x00000200,
        Union       = 0x00000400,
        Gemini      = 0x00000800,
        Tuner       = 0x00001000,
        Synchro     = 0x00002000,
        Token       = 0x00004000,
        Maximum     = 0x00008000,
        QuickPlay   = 0x00010000,
        Continuous  = 0x00020000,
        Equip       = 0x00040000,
        Field       = 0x00080000,
        Counter     = 0x00100000,
        Flip        = 0x00200000,
        Toon        = 0x00400000,
        Xyz         = 0x00800000,
        Pendulum    = 0x01000000,
        SPSummon    = 0x02000000,
        Link        = 0x04000000,
        Skill       = 0x08000000,
        Action      = 0x10000000,
        Plus        = 0x20000000,
        Minus       = 0x40000000,
        Armor       = 0x80000000,
        Tokens      = Monster | Normal | Token,
        Extra       = Fusion | Synchro | Xyz | Link
    };
    Q_DECLARE_FLAGS(CardType, CardTypeFlags)
    Q_DECLARE_OPERATORS_FOR_FLAGS(CardType)


    class CardInfo {
    public:
        explicit CardInfo();
        explicit CardInfo(const CardInfo &other) = default;
        explicit CardInfo(CardInfo &&other) = default;
        CardInfo &operator=(const CardInfo &other) = default;
        CardInfo &operator=(CardInfo &&other) = default;

        int id() const { return m_id; }
        void setId(int id);

        CardType cardType() const { return m_cardType; }
        void setCardType(CardType cardType);

        QString name() const { return m_name; }
        void setName(const QString &name);

        QString description() const { return m_description; }
        void setDescription(const QString &description);

    private:
        QString m_name;
        QString m_description;
        CardType m_cardType;
        int m_id;
    };

} // namespace ygo

#endif // CARDINFO_H
