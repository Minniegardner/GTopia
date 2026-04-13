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
    GamePlayer* GetPlayerByUserID(uint32 userID);
    GamePlayer* GetPlayerByRawName(const string& playerName);
    std::vector<GamePlayer*> FindPlayersByNamePrefix(const string& query, bool sameWorldOnly = false, uint32 worldID = 0) const;
    bool CanAccessCommand(GamePlayer* pPlayer, const CommandInfo& info) const;
    const std::vector<const CommandInfo*>& GetCommandInfos() const { return m_commandInfos; }

    void UpdatePlayers();
    void ForceSaveAllPlayers();

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
};

GameServer* GetGameServer();