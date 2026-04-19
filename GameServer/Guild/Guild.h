#pragma once

#include "Precompiled.h"
#include "Item/ItemUtils.h"

#include <vector>

enum class GuildPosition : uint32 {
    MEMBER = 0,
    ELDER = 1,
    CO_LEADER = 2,
    LEADER = 3
};

struct GuildMember {
    uint32 UserID = 0;
    GuildPosition Position = GuildPosition::MEMBER;
};

struct GuildInvite {
    uint32 UserID = 0;
    uint64 InvitedAtMS = 0;
};

class Guild {
public:
    Guild();
    ~Guild();

public:
    uint64 GetGuildID() const;
    void SetGuildID(uint64 guildID);

    uint32 GetOwnerID() const;
    void SetOwnerID(uint32 ownerID);

    uint32 GetWorldID() const;
    void SetWorldID(uint32 worldID);

    const std::string& GetName() const;
    void SetName(const std::string& name);

    const std::string& GetStatement() const;
    void SetStatement(const std::string& statement);

    uint16 GetLogoForeground() const;
    void SetLogoForeground(uint16 logoForeground);

    uint16 GetLogoBackground() const;
    void SetLogoBackground(uint16 logoBackground);

    uint32 GetLevel() const;
    void SetLevel(uint32 level);

    uint32 GetXP() const;
    void SetXP(uint32 xp);

    const std::vector<GuildMember>& GetMembers() const;
    void SetMembers(const std::vector<GuildMember>& members);
    void AddMember(const GuildMember& member);
    void RemoveMember(uint32 userID);
    GuildMember* GetMember(uint32 userID);

    bool IsShowMascot() const;
    void SetShowMascot(bool showMascot);

    uint64 GetCreatedAt() const;
    void SetCreatedAt(uint64 createdAtMS);

    const std::string& GetNotebook() const;
    void SetNotebook(const std::string& notebook);
    const std::vector<GuildInvite>& GetPendingInvites() const;
    void AddPendingInvite(uint32 userID, uint64 invitedAtMS);
    void RemovePendingInvite(uint32 userID);
    bool HasPendingInvite(uint32 userID) const;


private:
    uint64 m_createdAtMS = 0;
    uint64 m_guildID = 0;
    uint32 m_ownerID = 0;
    uint32 m_worldID = 0;
    uint32 m_level = 0;
    uint32 m_xp = 0;
    bool m_showMascot = true;
    std::string m_name;
    std::string m_statement;
    std::string m_notebook;
    uint16 m_logoForeground = ITEM_ID_BLANK;
    uint16 m_logoBackground = ITEM_ID_BLANK;
    std::vector<GuildMember> m_members;
    std::vector<GuildInvite> m_pendingInvites;
};
