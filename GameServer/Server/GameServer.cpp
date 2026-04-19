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

string GetItemNameSafe(uint32 itemID)
{
    ItemInfo* pItem = GetItemInfoManager()->GetItemByID(itemID);
    if(!pItem) {
        return "Item #" + ToString(itemID);
    }

    return pItem->name;
}

string GetWeekdayName(uint32 day)
{
    static const char* kNames[] = {
        "Sunday",
        "Monday",
        "Tuesday",
        "Wednesday",
        "Thursday",
        "Friday",
        "Saturday"
    };

    return kNames[day % 7];
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
#include "../Event/UDP/GameMessage/SetSkin.h"
#include "../Event/UDP/GameMessage/Respawn.h"
#include "../Event/UDP/GameMessage/MenuClickSocial.h"
#include "../Event/UDP/GameMessage/MenuClickInfo.h"
#include "../Event/UDP/GameMessage/MenuClickFavorite.h"
#include "../Event/UDP/GameMessage/MenuClickLeaderboards.h"
#include "../Event/UDP/GameMessage/MenuClickSecure.h"
#include "../Event/UDP/GameMessage/MenuClickRecycle.h"
#include "../Event/UDP/GameMessage/Store.h"
#include "../Event/UDP/GameMessage/StoreNavigate.h"
#include "../Event/UDP/GameMessage/Buy.h"
#include "../Event/UDP/GameMessage/Wrench.h"
#include "../Event/UDP/GameMessage/GrowID.h"
#include "../Event/UDP/GameMessage/TradeStarted.h"
#include "../Event/UDP/GameMessage/TradeCancel.h"
#include "../Event/UDP/GameMessage/TradeAccept.h"
#include "../Event/UDP/GameMessage/TradeRemove.h"
#include "../Event/UDP/GameMessage/TradeModify.h"

#include "../Command/RenderWorld.h"
#include "../Command/ClearInv.h"
#include "../Command/Clear.h"
#include "../Command/GiveItem.h"
#include "../Command/Ghost.h"
#include "../Command/Nick.h"
#include "../Command/SpawnGhost.h"
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
#include "../Command/SummonAll.h"
#include "../Command/Nuke.h"
#include "../Command/Suspend.h"
#include "../Command/ServerCmd.h"
#include "../Command/Maintenance.h"
#include "../Command/PBan.h"
#include "../Command/GrowIDCmd.h"
#include "../Command/SuperBroadcast.h"
#include "../Command/Weather.h"
#include "../Command/Vanish.h"
#include "../Command/Effect.h"
#include "../Command/SetGems.h"
#include "../Command/AddLevel.h"
#include "../Command/PInfo.h"
#include "../Command/AddRole.h"
#include "../Command/AddTitle.h"
#include "../Command/RemoveRole.h"
#include "../Command/Roles.h"
#include "../Command/ListRoles.h"
#include "../Command/GetRoles.h"
#include "../Command/InvSee.h"
#include "../Command/ItemInfo.h"
#include "../Command/ReplaceBlocks.h"
#include "../Command/RepairMagplantWorlds.h"
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
    if(!pPlayer) {
        return;
    }

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
    RegisterMessagePacket<SetSkin>(CompileTimeHashString("setSkin"));
    RegisterMessagePacket<Respawn>(CompileTimeHashString("respawn"));
    RegisterMessagePacket<Respawn>(CompileTimeHashString("respawn_spike"));
    RegisterMessagePacket<MenuClickSocial>(CompileTimeHashString("friends"));
    RegisterMessagePacket<MenuClickInfo>(CompileTimeHashString("info"));
    RegisterMessagePacket<MenuClickFavorite>(CompileTimeHashString("favorite"));
    RegisterMessagePacket<MenuClickLeaderboards>(CompileTimeHashString("leaderboards"));
    RegisterMessagePacket<MenuClickSecure>(CompileTimeHashString("secure"));
    RegisterMessagePacket<MenuClickRecycle>(CompileTimeHashString("recycle"));
    RegisterMessagePacket<Wrench>(CompileTimeHashString("wrench"));
    RegisterMessagePacket<Store>(CompileTimeHashString("store"));
    RegisterMessagePacket<StoreNavigate>(CompileTimeHashString("storenavigate"));
    RegisterMessagePacket<Buy>(CompileTimeHashString("buy"));
    RegisterMessagePacket<GrowID>(CompileTimeHashString("growid"));
    RegisterMessagePacket<TradeStarted>(CompileTimeHashString("trade_started"));
    RegisterMessagePacket<TradeStarted>(CompileTimeHashString("trade_start"));
    RegisterMessagePacket<TradeCancel>(CompileTimeHashString("trade_cancel"));
    RegisterMessagePacket<TradeAccept>(CompileTimeHashString("trade_accept"));
    RegisterMessagePacket<TradeRemove>(CompileTimeHashString("rem_trade"));
    RegisterMessagePacket<TradeModify>(CompileTimeHashString("mod_trade"));

    RegisterCommand<RenderWorld>();
    RegisterCommand<ClearInv>();
    RegisterCommand<Clear>();
    RegisterCommand<GiveItem>();
    RegisterCommand<Nick>();
    RegisterCommand<Ghost>();
    RegisterCommand<SpawnGhost>();
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
    RegisterCommand<SummonAll>();
    RegisterCommand<Nuke>();
    RegisterCommand<Suspend>();
    RegisterCommand<ServerCmd>();
    RegisterCommand<Maintenance>();
    RegisterCommand<PBan>();
    RegisterCommand<GrowIDCmd>();
    RegisterCommand<SuperBroadcast>();
    RegisterCommand<Weather>();
    RegisterCommand<Vanish>();
    RegisterCommand<Effect>();
    RegisterCommand<SetGems>();
    RegisterCommand<AddLevel>();
    RegisterCommand<PInfo>();
    RegisterCommand<AddRole>();
    RegisterCommand<AddTitle>();
    RegisterCommand<RemoveRole>();
    RegisterCommand<Roles>();
    RegisterCommand<ListRoles>();
    RegisterCommand<GetRoles>();
    RegisterCommand<InvSee>();
    RegisterCommand<ItemInfoCmd>();
    RegisterCommand<ReplaceBlocks>();
    RegisterCommand<RepairMagplantWorlds>();
}

void GameServer::UpdateGameLogic(uint64 maxTimeMS)
{
    ServerBase::UpdateGameLogic(maxTimeMS);
    UpdateMaintenance();
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
        const uint32 sourceUserID = data[4].GetUINT();
        const string sourceRawName = data[5].GetString();
        const string payloadText = data[6].GetString();
        const uint64 payloadNumber = (uint64)data[7].GetUINT();
        const uint32 sourceServerID = data.size() >= 11 ? data[10].GetUINT() : 0;

        if(actionType == TCP_CROSS_ACTION_SUPER_BROADCAST) {
            if(payloadText.empty()) {
                return;
            }

            for(auto& [_, pWorldPlayer] : m_playerCache) {
                if(!pWorldPlayer || !pWorldPlayer->HasState(PLAYER_STATE_IN_GAME)) {
                    continue;
                }

                pWorldPlayer->SendOnConsoleMessage(payloadText);
            }

            return;
        }

        if(actionType == TCP_CROSS_ACTION_SUMMON_ALL) {
            if(payloadText.empty()) {
                return;
            }

            for(auto& [_, pWorldPlayer] : m_playerCache) {
                if(!pWorldPlayer || !pWorldPlayer->HasState(PLAYER_STATE_IN_GAME)) {
                    continue;
                }

                if(pWorldPlayer->GetUserID() == sourceUserID) {
                    continue;
                }

                pWorldPlayer->SendOnConsoleMessage("`oYou were summoned by ``" + sourceRawName + "`` to `w" + payloadText + "``.");
                GetWorldManager()->PlayerJoinRequest(pWorldPlayer, payloadText);
            }

            return;
        }

        GamePlayer* pTarget = GetPlayerByUserID(targetUserID);
        if(!pTarget || !pTarget->HasState(PLAYER_STATE_IN_GAME)) {
            return;
        }

        switch(actionType) {
            case TCP_CROSS_ACTION_MSG: {
                if(payloadText.empty()) {
                    break;
                }

                pTarget->SendOnConsoleMessage("`o(From `$" + sourceRawName + "`o): " + payloadText);
                pTarget->PlaySFX("audio/pay_time.wav");
                break;
            }

            case TCP_CROSS_ACTION_SERVER_SWITCH: {
                auto parts = Split(payloadText, '|');
                if(parts.size() < 2) {
                    break;
                }

                uint32 serverPort = 0;
                if(ToUInt(parts[1], serverPort) != TO_INT_SUCCESS || serverPort == 0) {
                    break;
                }

                pTarget->SetSwitchingSubserver(true);
                pTarget->SendOnConsoleMessage("Redirecting..");
                pTarget->SendOnSendToServer(
                    (uint16)serverPort,
                    pTarget->GetLoginDetail().token,
                    pTarget->GetUserID(),
                    parts[0],
                    "",
                    "-1",
                    LOGIN_MODE_REDIRECT_SUBSERVER_SILENT
                );
                break;
            }

            case TCP_CROSS_ACTION_PINFO: {
                World* pTargetWorld = GetWorldManager()->GetWorldByID(pTarget->GetCurrentWorld());
                const string worldName = pTargetWorld ? pTargetWorld->GetWorlName() : "EXIT";
                const string info =
                    "`w" + pTarget->GetRawName() + "`` (UID: `o" + ToString(pTarget->GetUserID()) +
                    "`` | NID: `o" + ToString(pTarget->GetNetID()) +
                    "``) - LV `o" + ToString(pTarget->GetLevel()) +
                    "`` - Gems `o" + ToString(pTarget->GetGems()) +
                    "`` - Server `o" + ToString(GetContext()->GetID()) +
                    "`` - World `o" + worldName + "``";

                VariantVector result(8);
                result[0] = TCP_PACKET_CROSS_SERVER_ACTION;
                result[1] = TCP_CROSS_ACTION_RESULT;
                result[2] = actionType;
                result[3] = sourceUserID;
                result[4] = TCP_CROSS_ACTION_RESULT_OK;
                result[5] = pTarget->GetRawName();
                result[6] = info;
                result[7] = sourceServerID;
                GetMasterBroadway()->SendPacketRaw(result);
                break;
            }

            case TCP_CROSS_ACTION_WARPTO: {
                if(targetUserID == 0 || sourceUserID == 0) {
                    break;
                }

                const uint32 targetWorldID = pTarget->GetCurrentWorld();
                if(targetWorldID == 0) {
                    break;
                }

                World* pTargetWorld = GetWorldManager()->GetWorldByID(targetWorldID);
                if(!pTargetWorld || pTargetWorld->GetWorlName().empty()) {
                    break;
                }

                GetMasterBroadway()->SendCrossServerActionRequest(
                    TCP_CROSS_ACTION_SUMMON,
                    pTarget->GetUserID(),
                    pTarget->GetRawName(),
                    ToString(sourceUserID),
                    true,
                    pTargetWorld->GetWorlName(),
                    0);
                break;
            }

            case TCP_CROSS_ACTION_GIVEITEM: {
                uint32 itemID = 0;
                if(ToUInt(payloadText, itemID) != TO_INT_SUCCESS || itemID == 0) {
                    break;
                }

                ItemInfo* pItem = GetItemInfoManager()->GetItemByID(itemID);
                if(!pItem) {
                    break;
                }

                const uint32 giveAmount = payloadNumber > UINT32_MAX ? UINT32_MAX : (uint32)payloadNumber;
                if(giveAmount == 0) {
                    break;
                }

                const uint8 givenCount = pTarget->GetInventory().AddItem(pItem->id, giveAmount, pTarget);
                pTarget->SendOnConsoleMessage("`oGiven: " + ToString(givenCount) + "x " + pItem->name + " `oby ``" + sourceRawName + "``.");
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

            case TCP_CROSS_ACTION_PBAN: {
                const int32 durationSec = payloadNumber >= UINT32_MAX ? -1 : (int32)payloadNumber;
                pTarget->ApplyAccountBan(durationSec, payloadText, sourceRawName, false);
                break;
            }

            case TCP_CROSS_ACTION_MAINTENANCE: {
                InitiateMaintenance(payloadText, sourceUserID, sourceRawName, false);
                break;
            }

            case TCP_CROSS_ACTION_FRIEND_LOGIN:
            case TCP_CROSS_ACTION_FRIEND_LOGOUT: {
                if(!pTarget->IsShowFriendNotification()) {
                    break;
                }

                const bool loggedIn = actionType == TCP_CROSS_ACTION_FRIEND_LOGIN;
                if(!pTarget->ShouldProcessFriendAlert(sourceUserID, loggedIn, Time::GetSystemTime())) {
                    break;
                }

                pTarget->PlaySFX(loggedIn ? "friend_logon.wav" : "friend_logoff.wav", 2000);
                pTarget->SendOnConsoleMessage(
                    (loggedIn ? "`3FRIEND ALERT : `w" : "`!FRIEND ALERT : `w") +
                    sourceRawName +
                    (loggedIn ? " `ohas `2logged on`o." : " `ohas `4logged off`o.")
                );
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

            case TCP_CROSS_ACTION_PINFO:
                if(data.size() >= 7) {
                    pSource->SendOnConsoleMessage("`o>> `w" + data[6].GetString());
                }
                break;

            case TCP_CROSS_ACTION_SERVER_SWITCH:
                pSource->SendOnConsoleMessage("`oRedirecting to ``" + targetName + "``...");
                break;

            case TCP_CROSS_ACTION_SUMMON_ALL:
                pSource->SendOnConsoleMessage("`oSummon-all broadcast delivered across subservers.");
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

            case TCP_CROSS_ACTION_SUPER_BROADCAST:
                pSource->SendOnConsoleMessage("`oSuper-broadcast delivered across subservers.");
                break;

            case TCP_CROSS_ACTION_GIVEITEM:
                pSource->SendOnConsoleMessage("`oGiveitem delivered to ``" + targetName + "`` across subserver.");
                break;

            case TCP_CROSS_ACTION_WARPTO:
                pSource->SendOnConsoleMessage("`oWarp request sent to ``" + targetName + "`` across subserver.");
                break;

            case TCP_CROSS_ACTION_PBAN:
                pSource->SendOnConsoleMessage("`oApplied account ban to ``" + targetName + "`` across subserver.");
                break;

            case TCP_CROSS_ACTION_MAINTENANCE:
                pSource->SendOnConsoleMessage("`oMaintenance countdown started across subservers.");
                break;
        }

        return;
    }

    if(resultCode == TCP_CROSS_ACTION_RESULT_MULTIPLE_MATCH) {
        auto parts = Split(targetName, '|');
        if(parts.size() >= 2 && !parts[1].empty()) {
            pSource->SendOnConsoleMessage("`oThere are multiple players matching `w" + parts[0] + "`o globally, be more specific.");
            pSource->SendOnConsoleMessage("`oMatched players: `w" + parts[1] + "``");
        }
        else {
            pSource->SendOnConsoleMessage("`oThere are multiple players matching that name globally, be more specific.");
        }
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

    const string notFoundTarget = targetName.empty() ? "that query" : targetName;
    pSource->SendOnConsoleMessage("`6>> No one online who has a name starting with `$" + notFoundTarget + "`6.``");
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

    const TCPDailyQuestData& quest = GetMasterBroadway()->GetDailyQuestData();
    const string questLine = "`3Quest: Deliver `w" + ToString(quest.questItemOneAmount) + " " + GetItemNameSafe(quest.questItemOneID) + "`` + `w" + ToString(quest.questItemTwoAmount) + " " + GetItemNameSafe(quest.questItemTwoID) + "``.";
    const string rewardLine = "`3Rewards: `w" + ToString(quest.rewardOneAmount) + " " + GetItemNameSafe(quest.rewardOneID) + "`` + `w" + ToString(quest.rewardTwoAmount) + " " + GetItemNameSafe(quest.rewardTwoID) + "``.";
    const string msg = "`3Daily event synchronized across subservers: `#" + eventName + "`` (`9day=" + ToString(epochDay) + " seed=" + ToString(eventSeed) + "``)";
    for(auto& [_, pPlayer] : m_playerCache) {
        if(!pPlayer || !pPlayer->HasState(PLAYER_STATE_IN_GAME)) {
            continue;
        }

        pPlayer->SendOnConsoleMessage(msg);
        pPlayer->SendOnConsoleMessage(questLine);
        pPlayer->SendOnConsoleMessage(rewardLine);
    }
}

string GameServer::GetDailyEventStatusLine() const
{
    const uint32 eventType = GetMasterBroadway()->GetDailyEventType();
    const string eventName = GetDailyEventName(eventType);
    if(eventName.empty()) {
        return "";
    }

    const TCPDailyQuestData& quest = GetMasterBroadway()->GetDailyQuestData();
    const TCPWeeklyEventsData& weekly = GetMasterBroadway()->GetWeeklyEventsData();
    const TCPMonthlyEventsData& monthly = GetMasterBroadway()->GetMonthlyEventsData();

    string msg = "`3Today is `#" + eventName + "``.";

    if(quest.questItemOneID != 0 || quest.questItemTwoID != 0) {
        msg += " `oDaily quests: `w" + ToString(quest.questItemOneAmount) + " " + GetItemNameSafe(quest.questItemOneID) + "`` and `w" + ToString(quest.questItemTwoAmount) + " " + GetItemNameSafe(quest.questItemTwoID) + "``.";
        msg += " `oRewards: `w" + ToString(quest.rewardOneAmount) + " " + GetItemNameSafe(quest.rewardOneID) + "`` + `w" + ToString(quest.rewardTwoAmount) + " " + GetItemNameSafe(quest.rewardTwoID) + "``.";
    }

    msg += " `oRole quest cycle: Farmer `w" + GetWeekdayName(weekly.roleQuestFarmerDay) + "``, Builder `w" + GetWeekdayName(weekly.roleQuestBuilderDay) + "``.";
    msg += " `oThis month: Geiger day `w" + ToString(monthly.geigerDay) + "``, Ghost day `w" + ToString(monthly.ghostDay) + "``.";

    return msg;
}

bool GameServer::InitiateMaintenance(const string& message, uint32 sourceUserID, const string& sourceRawName, bool propagateToSubServers)
{
    if(m_isMaintenance) {
        return false;
    }

    const uint64 nowMS = Time::GetSystemTime();
    m_isMaintenance = true;
    m_isMarkedForMaintenance = false;
    m_hasMaintenanceAnnouncement = message.empty();
    m_sentMaintenance30 = false;
    m_sentMaintenance10 = false;
    m_sentMaintenanceZero = false;
    m_maintenanceEndTimeMS = nowMS + 10ull * 60ull * 1000ull;
    m_nextMaintenanceMinute = 9;
    m_nextMaintenanceMinuteTimeMS = nowMS + 60ull * 1000ull;
    m_maintenanceMessage = message;
    m_maintenanceSourceUserID = sourceUserID;
    m_maintenanceSourceRawName = sourceRawName;

    if(!message.empty()) {
        BroadcastMaintenanceMessage("** Global System Message: `4" + message, true);
        m_hasMaintenanceAnnouncement = true;
    }

    BroadcastMaintenanceMessage("`4Global System Message`o: Restarting server for update in `410 `ominutes", true);

    if(propagateToSubServers && GetMasterBroadway()->IsConnected()) {
        GetMasterBroadway()->SendCrossServerActionRequest(
            TCP_CROSS_ACTION_MAINTENANCE,
            sourceUserID,
            sourceRawName,
            "ALL",
            true,
            message,
            10
        );
    }

    return true;
}

void GameServer::BroadcastMaintenanceMessage(const string& message, bool playBeep)
{
    for(auto& [_, pPlayer] : m_playerCache) {
        if(!pPlayer) {
            continue;
        }

        pPlayer->SendOnConsoleMessage(message);
        if(playBeep) {
            pPlayer->PlaySFX("beep.wav", 0);
        }
    }
}

void GameServer::ForceDisconnectAllPlayers()
{
    std::vector<GamePlayer*> players;
    players.reserve(m_playerCache.size());

    for(auto& [_, pPlayer] : m_playerCache) {
        if(pPlayer) {
            players.push_back(pPlayer);
        }
    }

    for(GamePlayer* pPlayer : players) {
        pPlayer->LogOff();
    }
}

void GameServer::UpdateMaintenance()
{
    if(!m_isMaintenance) {
        return;
    }

    const uint64 nowMS = Time::GetSystemTime();

    while(m_nextMaintenanceMinute > 1 && nowMS >= m_nextMaintenanceMinuteTimeMS) {
        BroadcastMaintenanceMessage("`4Global System Message`o: Restarting server for update in `4" + ToString(m_nextMaintenanceMinute) + " `ominutes", true);
        --m_nextMaintenanceMinute;
        m_nextMaintenanceMinuteTimeMS += 60ull * 1000ull;
    }

    const uint64 remainingMS = nowMS >= m_maintenanceEndTimeMS ? 0 : (m_maintenanceEndTimeMS - nowMS);

    if(!m_isMarkedForMaintenance && remainingMS <= 60ull * 1000ull) {
        m_isMarkedForMaintenance = true;
        BroadcastMaintenanceMessage("`4Global System Message`o: Restarting server for update in `41 `ominute", true);
    }

    if(!m_sentMaintenance30 && remainingMS <= 30ull * 1000ull) {
        m_sentMaintenance30 = true;
        BroadcastMaintenanceMessage("`4Global System Message`o: Restarting server for update in `430 seconds", true);
    }

    if(!m_sentMaintenance10 && remainingMS <= 10ull * 1000ull) {
        m_sentMaintenance10 = true;
        BroadcastMaintenanceMessage("`4Global System Message`o: Restarting server for update in `410 seconds", true);
    }

    if(!m_sentMaintenanceZero && remainingMS == 0) {
        m_sentMaintenanceZero = true;
        BroadcastMaintenanceMessage("`4Global System Message`o: Restarting server for update in `4ZERO seconds! Should be back up in a minute or so. BYE!", true);
        ForceDisconnectAllPlayers();
        GetContext()->Shutdown();
    }
}

void GameServer::Kill()
{
    ServerBase::Kill();

    ForceSaveAllPlayers();

    GetItemInfoManager()->Kill();
    GetRoleManager()->Kill();
    GetWorldManager()->Kill();

    for(auto& [_, pPlayer] : m_playerCache) {
        SAFE_DELETE(pPlayer);
    }

    m_playerCache.clear();
}

GameServer* GetGameServer() { return GameServer::GetInstance(); }