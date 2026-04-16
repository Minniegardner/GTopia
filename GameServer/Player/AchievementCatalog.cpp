#include "AchievementCatalog.h"
#include "GamePlayer.h"
#include <algorithm>

namespace {

const std::vector<AchievementStatRule> kStatRules = {
    { "BLOCKS_PLACED", 100, "Builder (Classic)" },
    { "TREES_HARVESTED", 100, "Farmer (Classic)" },
    { "BLOCKS_DESTROYED", 100, "Demolition (Classic)" },
    { "SPENT_GEMS_ON_STORE", 10000, "Big Spender (Classic)" },
    { "ITEMS_TRASHED", 5000, "Trashman (Classic)" },
    { "CONSUMABLES_USED", 100, "Paint The Town Blue (Classic)" },
    { "LOCKS_PLACED", 1, "Mine, All Mine (Classic)" },
    { "COW_HARVESTED", 100, "Milkin' It (Classic)" },
    { "COMBINER_USED", 25, "Bubble Bubble (Classic)" },
    { "SURGERIES_DONE", 1, "Surgeon (Classic)" },
    { "FOUND_RADIOACTIVE_ITEMS", 1, "Radiation Hunter (Classic)" },
    { "STARSHIP_AIRLOCK_FALL", 100, "Deadly Vacuum (Classic)" },
    { "FISH_REVIVED", 10, "Resurrection (Classic)" },
    { "CAPTURED_GHOSTS", 100, "Who Ya Gonna Call? (Classic)" },
    { "CAPTURED_MIND_GHOSTS", 100, "Mastermind (Classic)" },
    { "BROKEN_ANOMALIZERS", 10, "Big Breaker (Classic)" },
    { "BROKEN_HAMMER_ANOMALIZERS", 10, "Hammer Time (Classic)" },
    { "BROKEN_SCYTHE_ANOMALIZERS", 10, "Reaper King (Classic)" },
    { "BROKEN_BONESAW_ANOMALIZERS", 10, "Bone Breaker (Classic)" },
    { "BROKEN_ANOMAROD", 10, "Rod Snapper (Classic)" },
    { "BROKEN_TROWEL_ANOMALIZERS", 10, "Trowel Troubles (Classic)" },
    { "BROKEN_CULTIVATOR_ANOMALIZERS", 10, "Cultivate This! (Classic)" },
    { "BROKEN_SCANNER_ANOMALIZERS", 10, "Scrapped Scanners! (Classic)" },
    { "BROKEN_ROLLING_PINS_ANOMALIZERS", 10, "Cooking Conundrum! (Classic)" }
};

const std::vector<string> kKnownAchievements = {
    "Packrat (Classic)",
    "Ding! (Classic)",
    "Builder (Classic)",
    "Farmer (Classic)",
    "Demolition (Classic)",
    "Big Spender (Classic)",
    "Trashman (Classic)",
    "Paint The Town Blue (Classic)",
    "Mine, All Mine (Classic)",
    "Milkin' It (Classic)",
    "Bubble Bubble (Classic)",
    "Surgeon (Classic)",
    "Radiation Hunter (Classic)",
    "Deadly Vacuum (Classic)",
    "Resurrection (Classic)",
    "Who Ya Gonna Call? (Classic)",
    "Mastermind (Classic)",
    "Big Breaker (Classic)",
    "Hammer Time (Classic)",
    "Reaper King (Classic)",
    "Bone Breaker (Classic)",
    "Rod Snapper (Classic)",
    "Trowel Troubles (Classic)",
    "Cultivate This! (Classic)",
    "Scrapped Scanners! (Classic)",
    "Cooking Conundrum! (Classic)",
    "Social Butterfly (Classic)",
    "Sparkly Dust (Classic)",
    "Embiggened (Classic)"
};

}

namespace AchievementCatalog {

const std::vector<AchievementStatRule>& GetStatRules()
{
    return kStatRules;
}

const std::vector<string>& GetKnownAchievements()
{
    return kKnownAchievements;
}

bool IsKnownAchievement(const string& achievementName)
{
    if(achievementName.empty()) {
        return false;
    }

    return std::find(kKnownAchievements.begin(), kKnownAchievements.end(), achievementName) != kKnownAchievements.end();
}

void ApplyStatAchievements(GamePlayer* pPlayer, const string& statName, uint64 statValue)
{
    if(!pPlayer || statName.empty()) {
        return;
    }

    for(const AchievementStatRule& rule : kStatRules) {
        if(statName != rule.statName) {
            continue;
        }

        if(statValue >= rule.threshold) {
            pPlayer->GiveAchievement(rule.achievementName);
        }

        return;
    }
}

void ApplyGemAchievements(GamePlayer* pPlayer, int32 currentGems)
{
    if(!pPlayer) {
        return;
    }

    if(currentGems >= 1000) {
        pPlayer->GiveAchievement("Packrat (Classic)");
    }
}

void ApplyLevelAchievements(GamePlayer* pPlayer, uint32 currentLevel)
{
    if(!pPlayer) {
        return;
    }

    if(currentLevel >= 10) {
        pPlayer->GiveAchievement("Ding! (Classic)");
    }
}

}
