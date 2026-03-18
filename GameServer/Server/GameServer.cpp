#include "GameServer.h"
#include "Packet/NetPacket.h"
#include "Packet/GamePacket.h"
#include "../Player/GamePlayer.h"
#include "Packet/PacketUtils.h"
#include "IO/Log.h"
#include "../World/WorldManager.h"

#include "../Event/UDP/GameMessage/RefreshItemData.h"
#include "../Event/UDP/GameMessage/EnterGame.h"
#include "../Event/UDP/GameMessage/JoinRequest.h"
#include "../Event/UDP/GameMessage/RefreshTributeData.h"
#include "../Event/UDP/GameMessage/Input.h"
#include "../Event/UDP/GameMessage/QuitToExit.h"
#include "../Event/UDP/GameMessage/DialogReturn.h"
#include "../Event/UDP/GameMessage/Trash.h"

#include "../Command/RenderWorld.h"
#include "../Command/GiveItem.h"
#include "../Command/Ghost.h"
#include "../Command/TogglePlayMod.h"
#include "../Command/Magic.h"

GameServer::GameServer()
{
}

GameServer::~GameServer()
{
    Kill();
    ServerBase::Kill();
}

void GameServer::OnEventConnect(ENetEvent& event)
{
    if(!event.peer) {
        return;
    }

    GamePlayer* pPlayer = new GamePlayer(event.peer);
    event.peer->data = pPlayer;

    m_playerCache.insert_or_assign(pPlayer->GetNetID(), pPlayer);

    pPlayer->SetState(PLAYER_STATE_LOGIN_REQUEST);
    pPlayer->SendHelloPacket();
}

void GameServer::OnEventReceive(ENetEvent& event)
{
    if(!event.peer) {
        return;
    }

    GamePlayer* pPlayer = (GamePlayer*)event.peer->data;
    if(event.peer != pPlayer->GetPeer()) {
        return;
    }

    uint32 msgType = GetMessageTypeFromEnetPacket(event.packet);
    LOGGER_LOG_DEBUG("%s", GetTextFromEnetPacket(event.packet));
    LOGGER_LOG_DEBUG("messageType: %d", msgType);

    switch(msgType) {
        case NET_MESSAGE_GENERIC_TEXT:
        case NET_MESSAGE_GAME_MESSAGE: {

            if(pPlayer->HasState(PLAYER_STATE_LOGIN_REQUEST)) {
                ParsedTextPacket<25> packet;
                ParseTextPacket(GetTextFromEnetPacket(event.packet), event.packet->dataLength - 4, packet);

                pPlayer->StartLoginRequest(packet);
                return;
            }
            else if(pPlayer->HasState(PLAYER_STATE_ENTERING_GAME)) {
                ParsedTextPacket<8> packet;
                ParseTextPacket(GetTextFromEnetPacket(event.packet), event.packet->dataLength - 4, packet);
            
                auto pAction = packet.Find(CompileTimeHashString("action"));
                if(pAction) {
                    uint32 packetType = HashString(pAction->value, pAction->size);

                    if(
                        packetType == CompileTimeHashString("refresh_item_data") ||
                        packetType == CompileTimeHashString("enter_game") ||
                        packetType == CompileTimeHashString("refresh_player_tribute_data")
                    ) {
                        m_messagePacket.Dispatch(packetType, pPlayer, packet);   
                    }
                }
                return;
            }

            if(
                !pPlayer->HasState(PLAYER_STATE_IN_GAME)
            ) {
                return;
            }
            
            ParsedTextPacket<8> packet; // increase it for dialog_return?
            ParseTextPacket(GetTextFromEnetPacket(event.packet), event.packet->dataLength - 4, packet);
        
            auto pAction = packet.Find(CompileTimeHashString("action"));
            if(pAction) {
                uint32 packetType = HashString(pAction->value, pAction->size);

                m_messagePacket.Dispatch(packetType, pPlayer, packet);
            }

            break;
        }

        case NET_MESSAGE_GAME_PACKET: {
            if(pPlayer->GetCurrentWorld() == 0) {
                /**
                 * response
                 */
                return;
            }

            GetWorldManager()->OnHandleGamePacket(event);
            break;
        }
    }
}

void GameServer::OnEventDisconnect(ENetEvent& event)
{
    if(!event.peer) {
        return;
    }

    GamePlayer* pPlayer = (GamePlayer*)event.peer->data;
    if(event.peer != pPlayer->GetPeer()) {
        return;
    }
    
    auto it = m_playerCache.find(pPlayer->GetNetID());
    if(it != m_playerCache.end()) {
        SAFE_DELETE(pPlayer);
        m_playerCache.erase(it);
    }
}

void GameServer::RegisterEvents()
{
    RegisterMessagePacket<RefreshItemData>(CompileTimeHashString("refresh_item_data"));
    RegisterMessagePacket<EnterGame>(CompileTimeHashString("enter_game"));
    RegisterMessagePacket<RefreshTributeData>(CompileTimeHashString("refresh_player_tribute_data"));
    RegisterMessagePacket<JoinRequest>(CompileTimeHashString("join_request"));
    RegisterMessagePacket<Input>(CompileTimeHashString("input"));
    RegisterMessagePacket<QuitToExit>(CompileTimeHashString("quit_to_exit"));
    RegisterMessagePacket<DialogReturn>(CompileTimeHashString("dialog_return"));
    RegisterMessagePacket<Trash>(CompileTimeHashString("trash"));

    RegisterCommand<RenderWorld>();
    RegisterCommand<GiveItem>();
    RegisterCommand<Ghost>();
    RegisterCommand<TogglePlayMod>();
    RegisterCommand<Magic>();
}

void GameServer::UpdateGameLogic(uint64 maxTimeMS)
{
    ServerBase::UpdateGameLogic(maxTimeMS);
    UpdatePlayers();
}

void GameServer::ExecuteCommand(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty()) {
        return;
    }

    uint32 hashCmd = HashString(args[0].substr(1));
    if(!m_commands.HasHandler(hashCmd)) {
        pPlayer->SendOnConsoleMessage("`4Unknown command. ``Enter `$/help`` for a list of valid commands.");
        return;
    }

    m_commands.Dispatch(
        HashString(args[0].substr(1)),
        pPlayer, args
    );
}

GamePlayer* GameServer::GetPlayerByUserID(uint32 userID)
{
    for(auto& [_, pPlayer] : m_playerCache) {
        if(pPlayer && pPlayer->GetUserID() == userID) {
            return pPlayer;
        }
    }

    return nullptr;
}

void GameServer::UpdatePlayers() // re-do
{
    if(m_playersLastUpdateTime.GetElapsedTime() < 16) {
        return;
    }

    for(auto& [_, pPlayer] : m_playerCache) {
        if(!pPlayer) {
            continue;
        }

        if(pPlayer->HasState(PLAYER_STATE_IN_GAME)) {
            pPlayer->Update();
        }
    }

    m_playersLastUpdateTime.Reset();
}

#include "Item/ItemInfoManager.h"
#include "Player/RoleManager.h"
void GameServer::Kill()
{
    ServerBase::Kill();

    GetItemInfoManager()->Kill();
    GetRoleManager()->Kill();

    for(auto& [_, pPlayer] : m_playerCache) {
        SAFE_DELETE(pPlayer);
    }

    m_playerCache.clear();
}

GameServer* GetGameServer() { return GameServer::GetInstance(); }