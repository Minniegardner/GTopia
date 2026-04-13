#pragma once

#include "../Precompiled.h"
#include "WorldTileManager.h"
#include "../Memory/MemoryBuffer.h"
#include "WorldObjectManager.h"

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

enum eWorldFlags
{
    WORLD_FLAG_NUKED = 1 << 0,
    WORLD_FLAG_RESTRICT_NOCLIP = 1 << 1,
    WORLD_FLAG_JAMMED = 1 << 2,
    WORLD_FLAG_PUNCH_JAMMER = 1 << 3,
    WORLD_FLAG_ZOMBIE_JAMMER = 1 << 4,
    WORLD_FLAG_ANTI_GRAVITY = 1 << 5,
    WORLD_FLAG_BALLOON_JAMMED = 1 << 6,
    WORLD_FLAG_MINI_MOD = 1 << 7,
    WORLD_FLAG_GUARDIAN_PINEAPPLE = 1 << 8,
    WORLD_FLAG_NOLOCKS = 1 << 9,
    WORLD_FLAG_HAUNTED = 1 << 10,
    WORLD_FLAG_NOGO = 1 << 11,
    WORLD_FLAG_NOEVENTS = 1 << 12
};

class WorldInfo {
public:
    WorldInfo();
    virtual ~WorldInfo();

public:
    bool Serialize(MemoryBuffer& memBuffer, bool write, bool database);
    void GenerateWorld(eWorldGenerationType type);

    void SetName(const string& worldName) { m_name = worldName; }
    const string& GetWorlName() const { return m_name; } 

    void SetCurrentWeather(uint32 newWeather) { m_currentWeather = newWeather; }
    uint32 GetCurrentWeather() const { return m_currentWeather; }

    void SetWorldFlag(uint32 flag, bool value) {
        if(value) {
            m_flags |= flag;
        }
        else {
            m_flags &= ~flag;
        }
    }

    void ToggleWorldFlag(uint32 flag) { m_flags ^= flag; }
    bool HasWorldFlag(uint32 flag) const { return (m_flags & flag) != 0; }
    uint32 GetWorldFlags() const { return m_flags; }

    uint32 GetDefaultWeather() const { return m_defaultWeather; }

    uint16 GetWorldVersion() const { return m_version; }

    WorldTileManager* GetTileManager() { return m_pTileMgr; };
    WorldObjectManager* GetObjectManager() { return m_pObjMgr; }

private:
    uint16 m_version;
    uint32 m_flags;
    string m_name;

    WorldTileManager* m_pTileMgr;
    WorldObjectManager* m_pObjMgr;

    uint32 m_defaultWeather;
    uint32 m_currentWeather;
};