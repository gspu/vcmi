/*
 * SideInBattle.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once
#include "../GameConstants.h"

VCMI_LIB_NAMESPACE_BEGIN

class CGHeroInstance;
class CArmedInstance;

struct DLL_LINKAGE SideInBattle
{
	PlayerColor color = PlayerColor::CANNOT_DETERMINE;
	const CGHeroInstance * hero = nullptr; //may be NULL if army is not commanded by hero
	const CArmedInstance * armyObject = nullptr; //adv. map object with army that participates in battle; may be same as hero

	uint32_t castSpellsCount = 0; //how many spells each side has been cast this turn
	std::vector<SpellID> usedSpellsHistory; //every time hero casts spell, it's inserted here -> eagle eye skill
	int32_t enchanterCounter = 0; //tends to pass through 0, so sign is needed

	void init(const CGHeroInstance * Hero, const CArmedInstance * Army);


	template <typename Handler> void serialize(Handler &h)
	{
		h & color;
		h & hero;
		h & armyObject;
		h & castSpellsCount;
		h & usedSpellsHistory;
		h & enchanterCounter;
	}
};

VCMI_LIB_NAMESPACE_END
