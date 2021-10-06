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
        // Remove inclusion/exclusion text.
        static const QRegularExpression re_inclusionExclusion(R"(\(This card('s name)? is (always|not) treated as (an? )?"[^"]+"( card)?\.\))");
        m_simplifiedEffect.remove(re_inclusionExclusion);

        // Remove extra deck materials.
        if (m_cardType & ygo::Extra && m_cardType & ygo::Monster) {
            static const QRegularExpression re_materials(R"(^[^\r\n]+(\r\n|\r|\n))");
            m_simplifiedEffect.remove(re_materials);
        }

        // Remove ritual spell card text.
        if (m_cardType & ygo::Ritual && m_cardType & ygo::Monster) {
            static const QRegularExpression re_ritualSpell1(R"(You can Ritual Summon this card with (a(ny)? )?"[^"]+"( Ritual Spell( Card)?| card)?\.)");
            m_simplifiedEffect.remove(re_ritualSpell1);
            static const QRegularExpression re_ritualSpell2(R"(This (card|monster) can only be Ritual Summoned with the Ritual Spell Card, "[^"]+"\.)");
            m_simplifiedEffect.remove(re_ritualSpell2);
        }

        // Remove gemini summoning condition text.
        if (m_cardType & ygo::Gemini && m_cardType & ygo::Monster) {
            static const QRegularExpression re_geminiCondition(R"(^[^●]+)");
            m_simplifiedEffect.remove(re_geminiCondition);
        }

        // Remove statlines of tokens and trap monsters.
        static const QRegularExpression re_statline(R"( \([^\)]+/[^\)]+/Level \d{1,2}/ATK \d+/DEF \d+\))");
        m_simplifiedEffect.remove(re_statline);

        // Remove type of card "(Monster, Spell, or Trap)".
        static const QRegularExpression re_cardType(R"( \(Monster, Spell,( (and/)?or)? Trap\))");
        m_simplifiedEffect.remove(re_cardType);

        // Remove type of monster card "(Ritual, Fusion, Synchro, Xyz, Pendulum, and Link)".
        static const QRegularExpression re_monsterCardType(R"( \((Ritual|Fusion|Synchro|Xyz|Pendulum|Link)(,?( and| or)? (Ritual|Fusion|Synchro|Xyz|Pendulum|Link))+\))");
        m_simplifiedEffect.remove(re_monsterCardType);

        // Remove "(but [its/their] effects can still be activated)".
        static const QRegularExpression re_stillBeActivated(R"( \(but (its|their) effects can still be activated\))");
        m_simplifiedEffect.remove(re_stillBeActivated);

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
