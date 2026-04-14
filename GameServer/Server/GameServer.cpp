#include "GameServer.h"
#include "MasterBroadway.h"
#include "Packet/NetPacket.h"
#include "Packet/GamePacket.h"
#include "../Player/GamePlayer.h"
#include "Packet/PacketUtils.h"
#include "IO/Log.h"
#include "../World/WorldManager.h"
#include "Item/ItemInfoManager.h"
#include "Player/RoleManager.h"
#include "../Context.h"
#include "Utils/StringUtils.h"
#include "Utils/Timer.h"

namespace {
string GetDailyEventName(uint32 eventType)
{
    switch(eventType) {
        case TCP_DAILY_EVENT_ROLE_JACK: return "Jack of all Trades Day";
        case TCP_DAILY_EVENT_ROLE_FARMER: return "Farmer Day";
        case TCP_DAILY_EVENT_ROLE_BUILDER: return "Builder Day";
        case TCP_DAILY_EVENT_ROLE_SURGEON: return "Surgeon Day";
        case TCP_DAILY_EVENT_ROLE_FISHER: return "Fishing Day";
        case TCP_DAILY_EVENT_ROLE_CHEF: return "Chef Day";
        case TCP_DAILY_EVENT_ROLE_STAR_CAPTAIN: return "Star Captain's Day";
        default: return "";
    }
}
}

#include "../Event/UDP/GameMessage/RefreshItemData.h"
#include "../Event/UDP/GameMessage/EnterGame.h"
#include "../Event/UDP/GameMessage/JoinRequest.h"
#include "../Event/UDP/GameMessage/RefreshTributeData.h"
#include "../Event/UDP/GameMessage/Input.h"
#include "../Event/UDP/GameMessage/QuitToExit.h"
#include "../Event/UDP/GameMessage/DialogReturn.h"
#include "../Event/UDP/GameMessage/Trash.h"
#include "../Event/UDP/GameMessage/Drop.h"
#include "../Event/UDP/GameMessage/Store.h"
#include "../Event/UDP/GameMessage/StoreNavigate.h"
#include "../Event/UDP/GameMessage/Buy.h"
#include "../Event/UDP/GameMessage/Wrench.h"
#include "../Event/UDP/GameMessage/GrowID.h"

#include "../Command/RenderWorld.h"
#include "../Command/GiveItem.h"
#include "../Command/Ghost.h"
#include "../Command/TogglePlayMod.h"
#include "../Command/Magic.h"
#include "../Command/Trade.h"
#include "../Command/Help.h"
#include "../Command/Kick.h"
#include "../Command/Pull.h"
#include "../Command/Mod.h"
#include "../Command/Find.h"
#include "../Command/Warp.h"
#include "../Command/Who.h"
#include "../Command/Online.h"
#include "../Command/Time.h"
#include "../Command/News.h"
#include "../Command/Stats.h"
#include "../Command/Ping.h"
#include "../Command/Msg.h"
#include "../Command/Reply.h"
#include "../Command/Ban.h"
#include "../Command/RandomPull.h"
#include "../Command/Warn.h"
#include "../Command/WarpTo.h"
#include "../Command/Uba.h"
#include "../Command/KickAll.h"
#include "../Command/Summon.h"
#include "../Command/Suspend.h"
#include "../Command/GrowIDCmd.h"
#include "../World/WorldManager.h"

GameServer::GameServer()
: NetEntity(NET_ID_GAME_SERVER)
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
    if(!pPlayer || event.peer != pPlayer->GetPeer()) {
        return;
    }

    uint32 msgType = GetMessageTypeFromEnetPacket(event.packet);

    switch(msgType) {
        case NET_MESSAGE_GENERIC_TEXT:
        case NET_MESSAGE_GAME_MESSAGE: {
            if(!pPlayer->CanProcessGameMessage()) {
                pPlayer->SendFakePingReply();
                return;
            }

            LOGGER_LOG_DEBUG("%s", GetTextFromEnetPacket(event.packet));

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

    pPlayer->LogOff();
    
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
    RegisterMessagePacket<Drop>(CompileTimeHashString("drop"));
    RegisterMessagePacket<Wrench>(CompileTimeHashString("wrench"));
    RegisterMessagePacket<Store>(CompileTimeHashString("store"));
    RegisterMessagePacket<StoreNavigate>(CompileTimeHashString("storenavigate"));
    RegisterMessagePacket<Buy>(CompileTimeHashString("buy"));
    RegisterMessagePacket<GrowID>(CompileTimeHashString("growid"));

    RegisterCommand<RenderWorld>();
    RegisterCommand<GiveItem>();
    RegisterCommand<Ghost>();
    RegisterCommand<TogglePlayMod>();
    RegisterCommand<Magic>();
    RegisterCommand<Trade>();
    RegisterCommand<Help>();
    RegisterCommand<Kick>();
    RegisterCommand<Pull>();
    RegisterCommand<Mod>();
    RegisterCommand<Find>();
    RegisterCommand<Warp>();
    RegisterCommand<Who>();
    RegisterCommand<Online>();
    RegisterCommand<CmdTime>();
    RegisterCommand<News>();
    RegisterCommand<Stats>();
    RegisterCommand<Ping>();
    RegisterCommand<Msg>();
    RegisterCommand<Reply>();
    RegisterCommand<Ban>();
    RegisterCommand<RandomPull>();
    RegisterCommand<Warn>();
    RegisterCommand<WarpTo>();
    RegisterCommand<Uba>();
    RegisterCommand<KickAll>();
    RegisterCommand<Summon>();
    RegisterCommand<Suspend>();
    RegisterCommand<GrowIDCmd>();
}

void GameServer::UpdateGameLogic(uint64 maxTimeMS)
{
    ServerBase::UpdateGameLogic(maxTimeMS);
    UpdatePlayers();
    GetWorldManager()->UpdateWorlds();
}

void GameServer::ExecuteCommand(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty()) {
        return;
    }

    uint32 hashCmd = HashString(args[0].substr(1));
    if(!m_commands.HasHandler(hashCmd)) {
        pPlayer->SendOnConsoleMessage("`4Unknown command.``  Enter `$/?`` for a list of valid commands.");
        return;
    }

    m_commands.Dispatch(
        HashString(args[0].substr(1)),
        pPlayer, args
    );
}

void GameServer::HandleCrossServerAction(VariantVector&& data)
{
    if(data.size() < 2) {
        return;
    }

    const int32 packetSubType = data[1].GetINT();

    if(packetSubType == TCP_CROSS_ACTION_EXECUTE) {
        if(data.size() < 10) {
            return;
        }

        const int32 actionType = data[2].GetINT();
        const uint32 targetUserID = data[3].GetUINT();
        const string sourceRawName = data[5].GetString();
        const string payloadText = data[6].GetString();
        const uint64 payloadNumber = (uint64)data[7].GetUINT();

        GamePlayer* pTarget = GetPlayerByUserID(targetUserID);
        if(!pTarget || !pTarget->HasState(PLAYER_STATE_IN_GAME)) {
            return;
        }

        switch(actionType) {
            case TCP_CROSS_ACTION_MSG: {
                pTarget->SendOnConsoleMessage("`o(From `$" + sourceRawName + "`o): " + payloadText);
                break;
            }

            case TCP_CROSS_ACTION_SUMMON: {
                pTarget->SendOnConsoleMessage("`oYou were summoned by ``" + sourceRawName + "`` to `w" + payloadText + "``.");
                GetWorldManager()->PlayerJoinRequest(pTarget, payloadText);
                break;
            }

            case TCP_CROSS_ACTION_SUSPEND: {
                const uint32 minutes = (uint32)payloadNumber;
                if(minutes == 0) {
                    break;
                }

                const uint64 mutedUntilMS = Time::GetSystemTime() + (uint64)minutes * 60ull * 1000ull;
                pTarget->SetMutedUntilMS(mutedUntilMS, payloadText);
                pTarget->SendOnConsoleMessage("`4You have been muted for `w" + ToString(minutes) + "`4 minute(s) by ``" + sourceRawName + "``. Reason: `w" + payloadText + "``");
                break;
            }

            case TCP_CROSS_ACTION_UNSUSPEND: {
                pTarget->ClearMute();
                pTarget->SendOnConsoleMessage("`oYour mute was removed by ``" + sourceRawName + "``.");
                break;
            }

            case TCP_CROSS_ACTION_KICK: {
                pTarget->SendOnConsoleMessage("`4You were kicked by ``" + sourceRawName + "``.");
                pTarget->LogOff();
                break;
            }

            case TCP_CROSS_ACTION_WARN: {
                pTarget->SendOnTextOverlay("`4WARNING:`` " + payloadText);
                pTarget->SendOnConsoleMessage("`4Warning from ``" + sourceRawName + "``: " + payloadText);
                break;
            }
        }

        return;
    }

    if(packetSubType != TCP_CROSS_ACTION_RESULT || data.size() < 6) {
        return;
    }

    const int32 actionType = data[2].GetINT();
    const uint32 sourceUserID = data[3].GetUINT();
    const int32 resultCode = data[4].GetINT();
    const string targetName = data[5].GetString();

    GamePlayer* pSource = GetPlayerByUserID(sourceUserID);
    if(!pSource || !pSource->HasState(PLAYER_STATE_IN_GAME)) {
        return;
    }

    if(resultCode == TCP_CROSS_ACTION_RESULT_OK) {
        switch(actionType) {
            case TCP_CROSS_ACTION_MSG:
                pSource->SendOnConsoleMessage("`o(Sent to `$" + targetName + "`o)");
                break;

            case TCP_CROSS_ACTION_SUMMON:
                pSource->SendOnConsoleMessage("`oSummon request sent for ``" + targetName + "`` across subserver.");
                break;

            case TCP_CROSS_ACTION_SUSPEND:
                pSource->SendOnConsoleMessage("`oMuted ``" + targetName + "`` across subserver.");
                break;

            case TCP_CROSS_ACTION_UNSUSPEND:
                pSource->SendOnConsoleMessage("`oUnmuted ``" + targetName + "`` across subserver.");
                break;

            case TCP_CROSS_ACTION_KICK:
                pSource->SendOnConsoleMessage("`oKicked ``" + targetName + "`` across subserver.");
                break;

            case TCP_CROSS_ACTION_WARN:
                pSource->SendOnConsoleMessage("`oWarned ``" + targetName + "`` across subserver.");
                break;
        }

        return;
    }

    if(resultCode == TCP_CROSS_ACTION_RESULT_MULTIPLE_MATCH) {
        pSource->SendOnConsoleMessage("`oThere are multiple players matching that name globally, be more specific.");
        return;
    }

    if(resultCode == TCP_CROSS_ACTION_RESULT_SELF_TARGET) {
        pSource->SendOnConsoleMessage("Nope, you can't use that on yourself.");
        return;
    }

    if(resultCode == TCP_CROSS_ACTION_RESULT_SEND_FAILED) {
        pSource->SendOnConsoleMessage("`4Cross-server action failed, target may have switched subserver.``");
        return;
    }

    pSource->SendOnConsoleMessage("`6>> No one online who has a name starting with `$" + targetName + "`6.``");
}

bool GameServer::CanAccessCommand(GamePlayer* pPlayer, const CommandInfo& info) const
{
    if(!pPlayer || info.disabled) {
        return false;
    }

    Role* pRole = pPlayer->GetRole();
    if(!pRole) {
        return false;
    }

    return pRole->HasPerm(info.perm);
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

GamePlayer* GameServer::GetPlayerByRawName(const string& playerName)
{
    for(auto& [_, pPlayer] : m_playerCache) {
        if(pPlayer && pPlayer->GetRawName() == playerName) {
            return pPlayer;
        }
    }

    return nullptr;
}

std::vector<GamePlayer*> GameServer::FindPlayersByNamePrefix(const string& query, bool sameWorldOnly, uint32 worldID) const
{
    std::vector<GamePlayer*> matches;
    const bool searchAll = query.empty();
    const string queryLower = searchAll ? string() : ToLower(query);

    for(const auto& [_, pPlayer] : m_playerCache) {
        if(!pPlayer || !pPlayer->HasState(PLAYER_STATE_IN_GAME)) {
            continue;
        }

        if(sameWorldOnly && pPlayer->GetCurrentWorld() != worldID) {
            continue;
        }

        const string rawNameLower = ToLower(pPlayer->GetRawName());

        if(searchAll) {
            matches.push_back(pPlayer);
            continue;
        }

        if(rawNameLower == queryLower) {
            return { pPlayer };
        }

        if(rawNameLower.size() < queryLower.size()) {
            continue;
        }

        if(rawNameLower.compare(0, queryLower.size(), queryLower) == 0) {
            matches.push_back(pPlayer);
        }
    }

    return matches;
}

void GameServer::UpdatePlayers()
{
    if(m_playersLastUpdateTime.GetElapsedTime() < TICK_INTERVAL) {
        return;
    }

    /**
     * maybe update a count of players per frame?
     * really needed that?
     */

    for(auto& [_, pPlayer] : m_playerCache) {
        if(!pPlayer) {
            continue;
        }

        if(pPlayer->HasState(PLAYER_STATE_IN_GAME)) {
            pPlayer->Update();

            if(pPlayer->GetLastDBSaveTime().GetElapsedTime() >= 15 * 60 * 1000) {
                pPlayer->SaveToDatabase();
                pPlayer->GetLastDBSaveTime().Reset();
            }
        }
    }

    m_playersLastUpdateTime.Reset();
}

void GameServer::ForceSaveAllPlayers()
{
    for(auto& [_, pPlayer] : m_playerCache) {
        if(!pPlayer) {
            continue;
        }

        pPlayer->SaveToDatabase();
    }
}

void GameServer::OnDailyEventSync(uint32 epochDay, uint32 eventType, uint32 eventSeed, bool announceToPlayers)
{
    if(!announceToPlayers) {
        return;
    }

    const string eventName = GetDailyEventName(eventType);
    if(eventName.empty()) {
        return;
    }

    const string msg = "`3Daily event synchronized across subservers: `#" + eventName + "`` (`9day=" + ToString(epochDay) + " seed=" + ToString(eventSeed) + "``)";
    for(auto& [_, pPlayer] : m_playerCache) {
        if(!pPlayer || !pPlayer->HasState(PLAYER_STATE_IN_GAME)) {
            continue;
        }

        pPlayer->SendOnConsoleMessage(msg);
    }
}

string GameServer::GetDailyEventStatusLine() const
{
    const uint32 eventType = GetMasterBroadway()->GetDailyEventType();
    const string eventName = GetDailyEventName(eventType);
    if(eventName.empty()) {
        return "";
    }

    return "`3Today is `#" + eventName + "``.";
}

void GameServer::Kill()
{
    ServerBase::Kill();

    GetItemInfoManager()->Kill();
    GetRoleManager()->Kill();
    GetWorldManager()->Kill();

    for(auto& [_, pPlayer] : m_playerCache) {
        SAFE_DELETE(pPlayer);
    }

    m_playerCache.clear();
}

GameServer* GetGameServer() { return GameServer::GetInstance(); }