#include "Msg.h"
#include "../Server/GameServer.h"
#include "../Server/MasterBroadway.h"
#include "Utils/StringUtils.h"

const CommandInfo& Msg::GetInfo()
{
    static CommandInfo info =
    {
        "Usage: /msg <`$full or first part of a name``> <`$your message``> - This will send a private message to someone anywhere in the universe.  If you don't include a message, you can just see if he/she is online or not.",
        "Send a private message",
        ROLE_PERM_NONE,
        {
            CompileTimeHashString("msg"),
            CompileTimeHashString("message")
        }
    };

    return info;
}

void Msg::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    if(args.size() < 2) {
        pPlayer->SendOnConsoleMessage("Usage: " + GetInfo().usage);
        return;
    }

    string query = args[1];
    bool exactMatch = false;
    if(!query.empty() && query[0] == '/') {
        exactMatch = true;
        query.erase(query.begin());
    }

    if(query.size() < 3 && !exactMatch) {
        pPlayer->SendOnConsoleMessage("`6>> `4Oops: `6Enter at least the `#first three characters `6of the persons name.``");
        return;
    }

    string message = args.size() >= 3 ? JoinString(args, " ", 2) : "";
    RemoveExtraWhiteSpaces(message);
    if(!message.empty() && message.size() > 180) {
        message.resize(180);
    }

    auto matches = GetGameServer()->FindPlayersByNamePrefix(query, false, 0);
    if(matches.empty()) {
        GetMasterBroadway()->SendCrossServerActionRequest(
            TCP_CROSS_ACTION_MSG,
            pPlayer->GetUserID(),
            pPlayer->GetRawName(),
            query,
            exactMatch,
            message,
            0);
        return;
    }

    if(!exactMatch && matches.size() > 1) {
        pPlayer->SendOnConsoleMessage("`oThere are multiple players starting with `w" + query + " `obe more specific.");
        return;
    }

    GamePlayer* pTarget = matches[0];
    if(!pTarget || pTarget == pPlayer) {
        pPlayer->SendOnConsoleMessage("Nope, you can't message yourself.");
        return;
    }

    if(message.empty()) {
        pPlayer->SendOnConsoleMessage("`6>> `w" + pTarget->GetRawName() + "`6 is online.");
        return;
    }

    const uint64 nowMS = Time::GetSystemTime();
    pPlayer->SetLastWhisperUserID(pTarget->GetUserID());
    pPlayer->SetLastWhisperAtMS(nowMS);
    pTarget->SetLastWhisperUserID(pPlayer->GetUserID());
    pTarget->SetLastWhisperAtMS(nowMS);

    pPlayer->SendOnConsoleMessage("`o(Sent to `$" + pTarget->GetRawName() + "`o)");
    pTarget->SendOnConsoleMessage("`o(From `$" + pPlayer->GetRawName() + "`o): " + message);
     pTarget->PlaySFX("pay_time.wav");
}
