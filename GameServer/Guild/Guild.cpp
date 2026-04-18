#include "Guild.h"

Guild::Guild()
{
}

Guild::~Guild()
{
}

uint64 Guild::GetGuildID() const
{
    return m_guildID;
}

void Guild::SetGuildID(uint64 guildID)
{
    m_guildID = guildID;
}

uint32 Guild::GetOwnerID() const
{
    return m_ownerID;
}

void Guild::SetOwnerID(uint32 ownerID)
{
    m_ownerID = ownerID;
}

uint32 Guild::GetWorldID() const
{
    return m_worldID;
}

void Guild::SetWorldID(uint32 worldID)
{
    m_worldID = worldID;
}

const std::string& Guild::GetName() const
{
    return m_name;
}

void Guild::SetName(const std::string& name)
{
    m_name = name;
}

const std::string& Guild::GetStatement() const
{
    return m_statement;
}

void Guild::SetStatement(const std::string& statement)
{
    m_statement = statement;
}

uint16 Guild::GetLogoForeground() const
{
    return m_logoForeground;
}

void Guild::SetLogoForeground(uint16 logoForeground)
{
    m_logoForeground = logoForeground;
}

uint16 Guild::GetLogoBackground() const
{
    return m_logoBackground;
}

void Guild::SetLogoBackground(uint16 logoBackground)
{
    m_logoBackground = logoBackground;
}

uint32 Guild::GetLevel() const
{
    return m_level;
}

void Guild::SetLevel(uint32 level)
{
    m_level = level;
}

uint32 Guild::GetXP() const
{
    return m_xp;
}

void Guild::SetXP(uint32 xp)
{
    m_xp = xp;
}

const std::vector<GuildMember>& Guild::GetMembers() const
{
    return m_members;
}

void Guild::SetMembers(const std::vector<GuildMember>& members)
{
    m_members = members;
}

void Guild::AddMember(const GuildMember& member)
{
    m_members.push_back(member);
}

void Guild::RemoveMember(uint32 userID)
{
    for(auto it = m_members.begin(); it != m_members.end(); ++it) {
        if(it->UserID == userID) {
            m_members.erase(it);
            break;
        }
    }
}

GuildMember* Guild::GetMember(uint32 userID)
{
    for(auto& member : m_members) {
        if(member.UserID == userID) {
            return &member;
        }
    }

    return nullptr;
}

bool Guild::IsShowMascot() const
{
    return m_showMascot;
}

void Guild::SetShowMascot(bool showMascot)
{
    m_showMascot = showMascot;
}

uint64 Guild::GetCreatedAt() const
{
    return m_createdAtMS;
}

void Guild::SetCreatedAt(uint64 createdAtMS)
{
    m_createdAtMS = createdAtMS;
}

const std::string& Guild::GetNotebook() const
{
    return m_notebook;
}

void Guild::SetNotebook(const std::string& notebook)
{
    m_notebook = notebook;
const std::vector<GuildInvite>& Guild::GetPendingInvites() const
{
    return m_pendingInvites;
}

void Guild::AddPendingInvite(uint32 userID, uint64 invitedAtMS)
{
    if(!HasPendingInvite(userID)) {
        m_pendingInvites.push_back({userID, invitedAtMS});
    }
}

void Guild::RemovePendingInvite(uint32 userID)
{
    for(auto it = m_pendingInvites.begin(); it != m_pendingInvites.end(); ++it) {
        if(it->UserID == userID) {
            m_pendingInvites.erase(it);
            break;
        }
    }
}

bool Guild::HasPendingInvite(uint32 userID) const
{
    for(const auto& invite : m_pendingInvites) {
        if(invite.UserID == userID) {
            return true;
        }
    }

    return false;
}
