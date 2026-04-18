#include "SuperBroadcast.h"

#include "Utils/StringUtils.h"
#include "Utils/Timer.h"
#include "../Server/GameServer.h"
#include "../Server/MasterBroadway.h"
#include "../World/WorldManager.h"
#include "../Player/GamePlayer.h"

const CommandInfo& SuperBroadcast::GetInfo()
{
    static CommandInfo info =
    {
        "/sb <message>",
        "Send a super broadcast to all players in all subservers",
        ROLE_PERM_NONE,
        {
            CompileTimeHashString("sb"),
            CompileTimeHashString("superbroadcast")
        }
    };

    return info;
}

void SuperBroadcast::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    string sentence = JoinString(args, " ", 1);
    RemoveExtraWhiteSpaces(sentence);
    if(sentence.empty()) {
        const int32 sbPrice = 50;
        const uint32 networkOnline = GetMasterBroadway()->GetGlobalOnlineCount();
        pPlayer->SendOnConsoleMessage("Usage: /sb <message>. Costs " + ToString(sbPrice) + " gems per online player. Current price: " + ToString(sbPrice * (int32)networkOnline) + " gems.");
        return;
    }

    const int32 sbPrice = 50;
    const uint32 networkOnline = GetMasterBroadway()->GetGlobalOnlineCount();
    const int32 totalPrice = sbPrice * (int32)networkOnline;

    if(pPlayer->GetGems() < totalPrice) {
        pPlayer->SendOnConsoleMessage("`4Oops: `oYou're currently short on gems. You need " + ToString(totalPrice) + " gems to broadcast!");
        return;
    }

    pPlayer->SetGems(pPlayer->GetGems() - totalPrice);
    pPlayer->SendOnSetBux();

    pPlayer->SendOnConsoleMessage("(Sent Super-Broadcast to all players. Broadcasts currently costs " + ToString(sbPrice) + " gems per player online. Charged you " + ToString(totalPrice) + " gems).");

    string worldName = "EXIT";
    if(pPlayer->GetCurrentWorld() != 0) {
        World* pWorld = GetWorldManager()->GetWorldByID(pPlayer->GetCurrentWorld());
        if(pWorld && !pWorld->GetWorlName().empty()) {
            worldName = pWorld->GetWorlName();
        }
    }

    char chatColor = 'o';
    if(pPlayer->GetRole() && pPlayer->GetRole()->GetChatColor() != 0) {
        chatColor = pPlayer->GetRole()->GetChatColor();
    }

    const string payloadText =
        "CP:_PL:0_OID:_CT:[SB]_ `#** `#from (``" + pPlayer->GetRawName() + "`#) in [`o" + worldName + "`#] ** :`" + string(1, chatColor) + " " + sentence;

    for(auto* pWorldPlayer : GetGameServer()->FindPlayersByNamePrefix("", false, 0)) {
        if(!pWorldPlayer || !pWorldPlayer->HasState(PLAYER_STATE_IN_GAME)) {
            continue;
        }

        pWorldPlayer->SendOnConsoleMessage(payloadText);
    }

    GetMasterBroadway()->SendCrossServerActionRequest(
        TCP_CROSS_ACTION_SUPER_BROADCAST,
        pPlayer->GetUserID(),
        pPlayer->GetRawName(),
        "*",
        true,
        payloadText,
        0
    );
}
