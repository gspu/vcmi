/*
* Goals.cpp, part of VCMI engine
*
* Authors: listed in file AUTHORS in main folder
*
* License: GNU General Public License v2.0 or later
* Full text of license available in license.txt file, in main folder
*
*/
#include "StdInc.h"
#include "RecruitHeroBehavior.h"
#include "../VCAI.h"
#include "../AIhelper.h"
#include "../AIUtility.h"
#include "../Goals/RecruitHero.h"
#include "../Goals/ExecuteHeroChain.h"
#include "lib/mapping/CMap.h" //for victory conditions
#include "lib/CPathfinder.h"

extern boost::thread_specific_ptr<CCallback> cb;
extern boost::thread_specific_ptr<VCAI> ai;
extern FuzzyHelper * fh;

using namespace Goals;

std::string RecruitHeroBehavior::toString() const
{
	return "Recruit hero";
}

Goals::TGoalVec RecruitHeroBehavior::getTasks()
{
	Goals::TGoalVec tasks;

	if(ai->canRecruitAnyHero())
	{
		if(cb->getHeroesInfo().size() < cb->getTownsInfo().size() + 1
			|| cb->getResourceAmount(Res::GOLD) > 10000)
		{
			tasks.push_back(Goals::sptr(Goals::RecruitHero()));
		}
	}

	return tasks;
}

std::string StartupBehavior::toString() const
{
	return "Startup";
}

const AIPath getShortestPath(const CGTownInstance * town, const std::vector<AIPath> & paths)
{
	auto shortestPath = *vstd::minElementByFun(paths, [town](const AIPath & path) -> float
	{
		if(town->garrisonHero && path.targetHero == town->garrisonHero.get())
			return 1;

		return path.movementCost();
	});

	return shortestPath;
}

const CGHeroInstance * getNearestHero(const CGTownInstance * town)
{
	auto paths = ai->ah->getPathsToTile(town->visitablePos());

	if(paths.empty())
		return nullptr;

	auto shortestPath = getShortestPath(town, paths);

	if(shortestPath.nodes.size() > 1 
		|| shortestPath.targetHero->visitablePos().dist2dSQ(town->visitablePos()) > 4
		|| town->garrisonHero && shortestPath.targetHero == town->garrisonHero.get())
		return nullptr;

	return shortestPath.targetHero;
}

Goals::TGoalVec StartupBehavior::getTasks()
{
	Goals::TGoalVec tasks;
	auto towns = cb->getTownsInfo();

	if(!towns.size())
		return tasks;

	const CGTownInstance * startupTown = towns.front();
	bool canRecruitHero = ai->canRecruitAnyHero(startupTown);
		
	if(towns.size() > 1)
	{
		startupTown = *vstd::maxElementByFun(towns, [](const CGTownInstance * town) -> float
		{
			auto closestHero = getNearestHero(town);

			if(!closestHero)
				return 0;

			return ai->ah->evaluateHero(closestHero);
		});
	}

	auto closestHero = getNearestHero(startupTown);

	if(closestHero)
	{
		if(!startupTown->visitingHero)
		{
			if(ai->ah->howManyReinforcementsCanGet(startupTown->getUpperArmy(), closestHero) > 200)
			{
				auto paths = ai->ah->getPathsToTile(startupTown->visitablePos());

				if(paths.size())
				{
					auto path = getShortestPath(startupTown, paths);

					tasks.push_back(Goals::sptr(Goals::ExecuteHeroChain(path, startupTown).setpriority(100)));
				}
			}
		}
		else
		{
			auto visitingHero = startupTown->visitingHero.get();
			auto visitingHeroScore = ai->ah->evaluateHero(visitingHero);
				
			if(startupTown->garrisonHero)
			{
				auto garrisonHero = startupTown->garrisonHero.get();
				auto garrisonHeroScore = ai->ah->evaluateHero(garrisonHero);

				if(garrisonHeroScore > visitingHeroScore)
				{
					if(ai->ah->howManyReinforcementsCanGet(garrisonHero, visitingHero) > 200)
						tasks.push_back(Goals::sptr(ExchangeSwapTownHeroes(startupTown, garrisonHero).setpriority(100)));
				}
				else
				{
					tasks.push_back(Goals::sptr(ExchangeSwapTownHeroes(startupTown, visitingHero).setpriority(100)));
				}
			}
			else if(canRecruitHero)
			{
				tasks.push_back(Goals::sptr(ExchangeSwapTownHeroes(startupTown, visitingHero).setpriority(100)));
			}
		}
	}

	if(tasks.empty() && canRecruitHero && !startupTown->visitingHero)
	{
		tasks.push_back(Goals::sptr(Goals::RecruitHero()));
	}

	if(tasks.empty() && towns.size())
	{
		for(const CGTownInstance * town : towns)
		{
			if(town->garrisonHero)
				tasks.push_back(Goals::sptr(ExchangeSwapTownHeroes(town, nullptr).setpriority(0.0001f)));
		}
	}

	return tasks;
}