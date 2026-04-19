#pragma once

#include "Guild.h"
#include <functional>

class GuildManager {
public:
    static GuildManager& GetInstance();

private:
    GuildManager();
    GuildManager(const GuildManager&) = delete;
    void operator=(const GuildManager&) = delete;

public:
    void Guilds(const std::function<void(uint64, Guild*)>& fn);
    uint32 GetGuildCount() const;
    Guild* Get(uint64 guildID);
    bool Add(Guild* guild);
    bool Remove(uint64 guildID);

private:
    std::unordered_map<uint64, Guild*> m_guilds;
};

inline GuildManager* GetGuildManager()
{
    return &GuildManager::GetInstance();
}
