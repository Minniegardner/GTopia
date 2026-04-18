#include "GuildManager.h"

GuildManager::GuildManager()
{
}

GuildManager& GuildManager::GetInstance()
{
    static GuildManager s_instance;
    return s_instance;
}

void GuildManager::Guilds(const std::function<void(uint64, Guild*)>& fn)
{
    for(auto& [guildID, guild] : m_guilds) {
        fn(guildID, guild);
    }
}

uint32 GuildManager::GetGuildCount() const
{
    return static_cast<uint32>(m_guilds.size());
}

Guild* GuildManager::Get(uint64 guildID)
{
    auto it = m_guilds.find(guildID);
    if(it == m_guilds.end()) {
        return nullptr;
    }

    return it->second;
}

bool GuildManager::Add(Guild* guild)
{
    if(!guild) {
        return false;
    }

    auto it = m_guilds.find(guild->GetGuildID());
    if(it != m_guilds.end()) {
        return false;
    }

    m_guilds[guild->GetGuildID()] = guild;
    return true;
}

bool GuildManager::Remove(uint64 guildID)
{
    auto it = m_guilds.find(guildID);
    if(it == m_guilds.end()) {
        return false;
    }

    SAFE_DELETE(it->second);
    m_guilds.erase(it);
    return true;
}
