#pragma once

#include "Precompiled.h"

class GamePlayer;

struct AchievementStatRule {
    const char* statName;
    uint64 threshold;
    const char* achievementName;
};

namespace AchievementCatalog {

const std::vector<AchievementStatRule>& GetStatRules();
const std::vector<string>& GetKnownAchievements();
bool IsKnownAchievement(const string& achievementName);

void ApplyStatAchievements(GamePlayer* pPlayer, const string& statName, uint64 statValue);
void ApplyGemAchievements(GamePlayer* pPlayer, int32 currentGems);
void ApplyLevelAchievements(GamePlayer* pPlayer, uint32 currentLevel);

}
