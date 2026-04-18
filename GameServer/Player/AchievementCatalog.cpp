#include "AchievementCatalog.h"
#include "GamePlayer.h"
#include <algorithm>

namespace {

const std::vector<AchievementStatRule> kStatRules = {
    { "BLOCKS_PLACED", 100, "Builder (Classic)" },
    { "BLOCKS_PLACED", 1000, "ProBuilder" },
    { "BLOCKS_PLACED", 10000, "ExpertBuilder" },
    { "TREES_HARVESTED", 100, "Farmer (Classic)" },
    { "TREES_HARVESTED", 1000, "FarmerInTheDell" },
    { "TREES_HARVESTED", 10000, "FarmasaurusRex" },
    { "BLOCKS_DESTROYED", 100, "Demolition (Classic)" },
    { "BLOCKS_DESTROYED", 1000, "WreckingCrew" },
    { "BLOCKS_DESTROYED", 10000, "Annihilator" },
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
    { "BROKEN_ANOMALIZERS", 100, "BiggerBreaker" },
    { "BROKEN_ANOMALIZERS", 500, "BiggestBreaker" },
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

const std::vector<string> kReferenceAchievementKeys = {
    "Builder", "Farmer", "Demolition", "Packrat", "BigSpender", "Trashman", "PaintTheTownBlue", "Embiggened", "MineAllMine", "Ding", "SocialButterfly", "YouLikeMe", "MilkinIt", "BubbleBubble", "Surgeron", "Legen", "SparkyDust", "RadiationHunter", "CrimeFighter", "DailyChallengeData", "DeadlyVacuum", "Resurrection", "WhoYaGonnaCall", "Mastermind", "BigBreaker", "HammerTime", "ReaperKing", "BoneBreaker", "RodSnapper", "TrowelTroubles", "CultivateThis", "ScappedScanners", "CookingConundrum", "ProBuilder", "FarmerInTheDell", "WreckingCrew", "Hoarder", "OlMoneybags", "GivinAHoot", "Splat", "SpaceCommander", "LetTheRightOneIn", "LongTimeFan", "SocialMonarch", "MooJuice", "Mixologist", "ChiefOfSurgery", "WaitForIt", "WeAreAllStardust", "GrowingAThirdArm", "SaviorOfTheCity", "MonthlyChallenge", "StarHoarder", "Bones", "AintAfraid", "MatterOverMind", "GrowmojiFan", "BiggerBreaker", "ExpertBuilder", "FarmasaurusRex", "Annihilator", "FilthyRich", "OneMeeelion", "OCD", "MadVandal", "MasterOfSpaceAndTime", "ThisLandIsMyLand", "ObsessiveGrowtopian", "Supporter", "KevinBacon", "BestOfAllPossibleWorlds", "KingOfTheWorld", "CollectorExtraordinaire", "Science", "Superhero", "SurgeronGeneral", "Dary", "Intergalactic", "AtomicLuck", "HackedTheServer", "Fishmaster", "FailureChallenged", "TeaEarlGreyHot", "RunninOnEmpty", "FinFan", "Ghosted", "GrowmojiMaster", "BiggestBreaker", "Crafty", "GuildLeader", "StarBling", "InterstellarChampion", "SuperScience", "Berserk", "TheSpecial", "CrazyFarmer", "Millionaire", "Void", "MadShopper", "Showoff", "SuperSupporter", "ProvidersWillProvide", "TheDoctor", "Consumer", "ChemicalCreator", "WildFireSavior", "AncestralBeing", "BossedAround", "MasterBuilder", "MasterChef", "MasterFarmer", "MasterSurgeon", "MasterStarCaptain", "MasterFisher", "ChemicalCrusader", "ViciousVictory", "Ascended", "Descended", "AHigherPower", "BossGhostToast", "AngelOfMercy", "AscendedUniverse", "DescendedUniverse", "PowerOverwhelming", "JackOfAllTrades", "BigShowoff", "MedicalMarvel", "InterstellarTycoon", "CelebrateGoodTimes", "LifeOfTheParty", "FirstBirthday", "GrowtopianOfTheYear", "SecondBirthday", "FifthAnniversary", "SixthAnniversary", "SeventhAnniversary", "EightAnniversary", "Heartbreaker", "StupidCupid", "FourLeaves", "LittleGreenMan", "GotLuckyCharms", "SixteenDozen", "BouncingBabyBunny", "ADeadRabbit", "EggHunter", "BashCinco", "LaVidaDeLaFiesta", "Gorro", "Champeon", "TooManyPineapples", "FreshAir", "TheLastCelebration", "SummerGrillin", "BrightFuture", "HarvesterOfWorlds", "Sacrifice", "CostumeContest", "SpiritOfGiving", "DeerHunter", "BigDonor", "CrachShot", "DiscipleOfGrowganoth", "ConcentratedPowerOfWill", "SevenYearsGoodLuck", "Ringu", "Wasted", "DeathRacer", "TheBrutalestBounce", "SpikySurvivor"
};

}

namespace AchievementCatalog {

const std::vector<AchievementStatRule>& GetStatRules()
{
    return kStatRules;
}

const std::vector<string>& GetKnownAchievements()
{
    static std::vector<string> merged;
    if(merged.empty()) {
        merged.reserve(kKnownAchievements.size() + kReferenceAchievementKeys.size());

        auto appendUnique = [&](const std::vector<string>& src) {
            for(const string& name : src) {
                if(std::find(merged.begin(), merged.end(), name) == merged.end()) {
                    merged.push_back(name);
                }
            }
        };

        appendUnique(kKnownAchievements);
        appendUnique(kReferenceAchievementKeys);
    }

    return merged;
}

bool IsKnownAchievement(const string& achievementName)
{
    if(achievementName.empty()) {
        return false;
    }

    const std::vector<string>& known = GetKnownAchievements();
    return std::find(known.begin(), known.end(), achievementName) != known.end();
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
    if(currentGems >= 10000) {
        pPlayer->GiveAchievement("Hoarder");
    }
    if(currentGems >= 100000) {
        pPlayer->GiveAchievement("OlMoneybags");
    }
    if(currentGems >= 500000) {
        pPlayer->GiveAchievement("FilthyRich");
    }
    if(currentGems >= 1000000) {
        pPlayer->GiveAchievement("OneMeeelion");
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
