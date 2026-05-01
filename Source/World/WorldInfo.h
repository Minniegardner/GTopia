#pragma once

#include "../Precompiled.h"
#include "WorldTileManager.h"
#include "../Memory/MemoryBuffer.h"
#include "WorldObjectManager.h"

enum eWorldCategory
{
    WORLD_CATEGORY_DEFAULT,
    WORLD_CATEGORY_TOP_WORLDS,
    WORLD_CATEGORY_RANDOM,
    WORLD_CATEGORY_YOUR_WORLDS,
    WORLD_CATEGORY_ADVENTURE,
    WORLD_CATEGORY_ART,
    WORLD_CATEGORY_FARM,
    WORLD_CATEGORY_GAME,
    WORLD_CATEGORY_GUILD,
    WORLD_CATEGORY_INFORMATION,
    WORLD_CATEGORY_MUSIC,
    WORLD_CATEGORY_PARKOUR,
    WORLD_CATEGORY_PUZZLE,
    WORLD_CATEGORY_ROLEPLAY,
    WORLD_CATEGORY_SHOP,
    WORLD_CATEGORY_SOCIAL,
    WORLD_CATEGORY_STORAGE,
    WORLD_CATEGORY_STORY,
    WORLD_CATEGORY_TRADE
};

inline const char* WorldCategoryToString(eWorldCategory category)
{
    switch(category) {
        case WORLD_CATEGORY_DEFAULT: return "Default";
        case WORLD_CATEGORY_TOP_WORLDS: return "Top Worlds";
        case WORLD_CATEGORY_RANDOM: return "Random";
        case WORLD_CATEGORY_YOUR_WORLDS: return "Your Worlds";
        case WORLD_CATEGORY_ADVENTURE: return "Adventure";
        case WORLD_CATEGORY_ART: return "Art";
        case WORLD_CATEGORY_FARM: return "Farm";
        case WORLD_CATEGORY_GAME: return "Game";
        case WORLD_CATEGORY_GUILD: return "Guild";
        case WORLD_CATEGORY_INFORMATION: return "Information";
        case WORLD_CATEGORY_MUSIC: return "Music";
        case WORLD_CATEGORY_PARKOUR: return "Parkour";
        case WORLD_CATEGORY_PUZZLE: return "Puzzle";
        case WORLD_CATEGORY_ROLEPLAY: return "Roleplay";
        case WORLD_CATEGORY_SHOP: return "Shop";
        case WORLD_CATEGORY_SOCIAL: return "Social";
        case WORLD_CATEGORY_STORAGE: return "Storage";
        case WORLD_CATEGORY_STORY: return "Story";
        case WORLD_CATEGORY_TRADE: return "Trade";
    }

    return "Default";
}

enum eWorldGenerationType
{
    WORLD_GENERATION_DEFAULT = 0,
    WORLD_GENERATION_CLEAR = 1
};

enum eWeatherTypes 
{
    WEATHER_TYPE_DEFAULT = 0,
    WEATHER_TYPE_SUNSET = 1,
    WEATHER_TYPE_NIGHT = 2,
    WEATHER_TYPE_DESERT = 3,
    WEATHER_TYPE_SUNNY = 4,
    WEATHER_TYPE_RAINY_CITY = 5,
    WEATHER_TYPE_HARVEST = 6
};

bool IsValidWorldName(const string& worldName);

class WorldInfo {
public:
    WorldInfo();
    virtual ~WorldInfo();

public:
    void Kill();

    bool Serialize(MemoryBuffer& memBuffer, bool write, bool database);
    void GenerateWorld(eWorldGenerationType type);
    uint32 GetMemEstimate(bool database);

    void SetName(const string& worldName) { m_name = worldName; }
    const string& GetWorlName() const { return m_name; } 

    void SetCategory(eWorldCategory category) { m_category = category; }
    eWorldCategory GetCategory() const { return m_category; }
    string GetWorldInfoString() const;

    void SetCurrentWeather(uint32 newWeather) { m_currentWeather = newWeather; }
    uint32 GetCurrentWeather() const { return m_currentWeather; }

    uint32 GetDefaultWeather() const { return m_defaultWeather; }

    uint16 GetWorldVersion() const { return m_version; }

    WorldTileManager* GetTileManager() { return m_pTileMgr; };
    WorldObjectManager* GetObjectManager() { return m_pObjMgr; }

private:
    uint16 m_version;
    uint32 m_flags;
    string m_name;
    eWorldCategory m_category;

    WorldTileManager* m_pTileMgr;
    WorldObjectManager* m_pObjMgr;

    uint32 m_defaultWeather;
    uint32 m_currentWeather;
};