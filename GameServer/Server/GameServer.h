#pragma once

#include "Server/ServerBase.h"
#include "Event/EventDispatcher.h"
#include "../Player/GamePlayer.h"
#include "../Command/CommandBase.h"

#include <algorithm>

class GameServer : public ServerBase, NetEntity {
public:
    GameServer();
    ~GameServer();

public:
    static GameServer* GetInstance() {
        static GameServer instance;
        return &instance;
    }

public:
    void OnEventConnect(ENetEvent& event) override;
    void OnEventReceive(ENetEvent& event) override;
    void OnEventDisconnect(ENetEvent& event) override;
    void RegisterEvents() override;
    void Kill() override;
    void UpdateGameLogic(uint64 maxTimeMS) override;

public:
    void ExecuteCommand(GamePlayer* pPlayer, std::vector<string>& args);
    void HandleCrossServerAction(VariantVector&& data);
    bool InitiateMaintenance(const string& message, uint32 sourceUserID = 0, const string& sourceRawName = "", bool propagateToSubServers = true);
    bool IsMaintenance() const { return m_isMaintenance; }
    bool IsMarkedForMaintenance() const { return m_isMarkedForMaintenance; }
    GamePlayer* GetPlayerByUserID(uint32 userID);
    GamePlayer* GetPlayerByRawName(const string& playerName);
    std::vector<GamePlayer*> FindPlayersByNamePrefix(const string& query, bool sameWorldOnly = false, uint32 worldID = 0) const;
    bool CanAccessCommand(GamePlayer* pPlayer, const CommandInfo& info) const;
    const std::vector<const CommandInfo*>& GetCommandInfos() const { return m_commandInfos; }

    void UpdatePlayers();
    void UpdateMaintenance();
    void BroadcastMaintenanceMessage(const string& message, bool playBeep = true);
    void ForceDisconnectAllPlayers();
    void ForceSaveAllPlayers();
    void OnDailyEventSync(uint32 epochDay, uint32 eventType, uint32 eventSeed, bool announceToPlayers);
    string GetDailyEventStatusLine() const;

    uint32 GetOnlineCount() { return m_playerCache.size(); }

private:
    template<class T>
    void RegisterMessagePacket(uint32 eventHash)
    {
        m_messagePacket.Register(
            eventHash,
            Delegate<GamePlayer*, ParsedTextPacket<8>&>::Create<&T::Execute>()
        );
    }

    template<class T>
    void RegisterCommand()
    {
        const CommandInfo* pInfo = &T::GetInfo();
        if(std::find(m_commandInfos.begin(), m_commandInfos.end(), pInfo) == m_commandInfos.end()) {
            m_commandInfos.push_back(pInfo);
        }

        for(auto& alias : T::GetInfo().aliases) {
            m_commands.Register(
                alias,
                Delegate<GamePlayer*, std::vector<string>&>::Create<&T::Execute>()
            );
        }
    }

private:
    EventDispatcher<uint32, GamePlayer*, ParsedTextPacket<8>&> m_messagePacket;
    EventDispatcher<uint32, GamePlayer*, std::vector<string>&> m_commands;
    std::vector<const CommandInfo*> m_commandInfos;

    Timer m_playersLastUpdateTime;
    bool m_isMaintenance = false;
    bool m_isMarkedForMaintenance = false;
    bool m_hasMaintenanceAnnouncement = false;
    bool m_sentMaintenance30 = false;
    bool m_sentMaintenance10 = false;
    bool m_sentMaintenanceZero = false;
    uint64 m_maintenanceEndTimeMS = 0;
    uint64 m_nextMaintenanceMinuteTimeMS = 0;
    int32 m_nextMaintenanceMinute = 10;
    string m_maintenanceMessage;
    uint32 m_maintenanceSourceUserID = 0;
    string m_maintenanceSourceRawName;
};

GameServer* GetGameServer();